#include "sock_cb_freertos.h"

#include <string.h>

#include <FreeRTOS_DNS.h>
#include <task.h>

#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/x509_crt.h>

#include <khc.h>

#include "log.h"
#include "random.h"

#if defined(MBEDTLS_HAVE_TIME_DATE)
#error "mbedTLS would check the certificates' dates at the system's time, which a device without a real-time clock does not have, and refuse them: unset MBEDTLS_HAVE_TIME_DATE. They are checked at the clock's time here."
#endif

/* For connecting and the handshake, which take seconds under QEMU. The
   connection's own timeouts are set after. */
#define HANDSHAKE_TIMEOUT_MS 30000
/* For the server to close its side once this one has. */
#define SHUTDOWN_TIMEOUT_MS 2000

static mbedtls_x509_crt ca_chain;
static mbedtls_ssl_config tls_config;

int sock_cb_init_tls(const char* ca_pem, size_t ca_pem_size)
{
    mbedtls_x509_crt_init(&ca_chain);
    mbedtls_ssl_config_init(&tls_config);
    int ret = mbedtls_x509_crt_parse(&ca_chain, (const unsigned char*)ca_pem, ca_pem_size);
    if (ret > 0) {
        /* How many of the certificates it could not parse. */
        return MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT;
    } else if (ret < 0) {
        return ret;
    }
    ret = mbedtls_ssl_config_defaults(&tls_config, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        return ret;
    }
    /* The server's certificate has to chain to one of the CA certificates, and
       be issued for the host, which sock_cb_connect() sets. */
    mbedtls_ssl_conf_authmode(&tls_config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls_config, &ca_chain, NULL);
    mbedtls_ssl_conf_rng(&tls_config, random_bytes, NULL);
    return 0;
}

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send,
        kii_clock_t* clock)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->socket = FREERTOS_INVALID_SOCKET;
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
    ctx->clock = clock;
}

static void set_timeouts(Socket_t socket, TickType_t to_recv, TickType_t to_send)
{
    FreeRTOS_setsockopt(socket, 0, FREERTOS_SO_RCVTIMEO, &to_recv, sizeof(to_recv));
    FreeRTOS_setsockopt(socket, 0, FREERTOS_SO_SNDTIMEO, &to_send, sizeof(to_send));
}

static TickType_t seconds_to_ticks(unsigned int seconds)
{
    return seconds == 0 ? portMAX_DELAY : pdMS_TO_TICKS(seconds * 1000u);
}

/* mbedTLS's input and output, on the FreeRTOS-Plus-TCP socket. A timeout ends
   a send or a receive with nothing done, which mbedTLS is told is wanting to
   write or read again. */
static int bio_send(void* socket, const unsigned char* buffer, size_t length)
{
    BaseType_t ret = FreeRTOS_send((Socket_t)socket, buffer, length, 0);
    if (ret > 0) {
        return (int)ret;
    } else if (ret == 0 || ret == -pdFREERTOS_ERRNO_ENOSPC) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return MBEDTLS_ERR_NET_SEND_FAILED;
}

static int bio_recv(void* socket, unsigned char* buffer, size_t length)
{
    BaseType_t ret = FreeRTOS_recv((Socket_t)socket, buffer, length, 0);
    if (ret > 0) {
        return (int)ret;
    } else if (ret == 0) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    } else if (ret == -pdFREERTOS_ERRNO_ENOTCONN) {
        /* The server closed the connection: the end of the stream. */
        return 0;
    }
    return MBEDTLS_ERR_NET_RECV_FAILED;
}

/* The validity period of a certificate, against the clock. */
static kii_validity_t check_dates(kii_clock_t* clock, const mbedtls_x509_crt* crt)
{
    const mbedtls_x509_time* from = &crt->valid_from;
    const mbedtls_x509_time* to = &crt->valid_to;
    int64_t not_before = 0;
    int64_t not_after = 0;
    if (!khc_seconds_since_epoch(from->year, from->mon, from->day,
                from->hour, from->min, from->sec, &not_before) ||
            !khc_seconds_since_epoch(to->year, to->mon, to->day,
                to->hour, to->min, to->sec, &not_after)) {
        /* A date that is not one. */
        return KII_VALIDITY_EXPIRED;
    }
    return kii_clock_check_validity(clock, not_before, not_after);
}

/* mbedTLS calls this for each certificate of the chain it verified, from the
   CA's to the server's, with what it found wrong with it in flags. It checks
   all but their validity periods, which this checks at the clock's time: its
   floor until a server has stated the time, which only tells a certificate
   that has expired, and that time from then on. */
static int verify_dates(void* clock, mbedtls_x509_crt* crt, int depth, uint32_t* flags)
{
    (void)depth;
    switch (check_dates((kii_clock_t*)clock, crt)) {
        case KII_VALIDITY_OK:
            break;
        case KII_VALIDITY_NOT_YET_VALID:
            *flags |= MBEDTLS_X509_BADCERT_FUTURE;
            break;
        default:
            *flags |= MBEDTLS_X509_BADCERT_EXPIRED;
            break;
    }
    return 0;
}

static void log_tls_error(const char* what, const char* host, int ret)
{
    char description[96];
    mbedtls_strerror(ret, description, sizeof(description));
    sample_log("%s %s: -0x%04x, %s", what, host, (unsigned int)-ret, description);
}

/* A certificate not valid yet at a time the clock had as exact is refused, and
   the time forgotten: more likely than a certificate from the future, the time
   fell behind, the tick having stopped while the device slept and the server
   having renewed its certificate meanwhile. Checked against a lower bound, the
   next connection goes through, and its response gives the clock the time
   again. */
static void log_refused(socket_context_t* ctx, const char* host)
{
    uint32_t flags = mbedtls_ssl_get_verify_result(&ctx->ssl);
    if (flags & MBEDTLS_X509_BADCERT_FUTURE) {
        sample_log("server certificate for %s refused: not valid yet", host);
        kii_clock_forget_time(ctx->clock);
    } else if (flags & MBEDTLS_X509_BADCERT_EXPIRED) {
        sample_log("server certificate for %s refused: expired", host);
    } else {
        char reason[128];
        if (mbedtls_x509_crt_verify_info(reason, sizeof(reason), "", flags) < 0) {
            reason[0] = '\0';
        }
        /* The first of the reasons, a line each. */
        reason[strcspn(reason, "\n")] = '\0';
        sample_log("server certificate for %s refused: %s", host, reason);
    }
}

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            uint16_t port)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    uint32_t address = FreeRTOS_gethostbyname(host);
    if (address == 0) {
        sample_log("failed to resolve %s", host);
        return KHC_SOCK_FAIL;
    }
    ctx->socket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM,
            FREERTOS_IPPROTO_TCP);
    if (ctx->socket == FREERTOS_INVALID_SOCKET) {
        sample_log("failed to create a socket");
        return KHC_SOCK_FAIL;
    }
    const TickType_t handshake_timeout = pdMS_TO_TICKS(HANDSHAKE_TIMEOUT_MS);
    set_timeouts(ctx->socket, handshake_timeout, handshake_timeout);
    struct freertos_sockaddr server;
    memset(&server, 0, sizeof(server));
    server.sin_len = sizeof(server);
    server.sin_family = FREERTOS_AF_INET;
    server.sin_port = FreeRTOS_htons(port);
    server.sin_address.ulIP_IPv4 = address;
    BaseType_t err = FreeRTOS_connect(ctx->socket, &server, sizeof(server));
    if (err != 0) {
        sample_log("failed to connect to %s:%u: %ld", host, port, (long)err);
        goto close_socket;
    }

    mbedtls_ssl_init(&ctx->ssl);
    int ret = mbedtls_ssl_setup(&ctx->ssl, &tls_config);
    if (ret == 0) {
        ret = mbedtls_ssl_set_hostname(&ctx->ssl, host);
    }
    if (ret != 0) {
        log_tls_error("failed to set up TLS for", host, ret);
        goto free_tls;
    }
    mbedtls_ssl_set_verify(&ctx->ssl, verify_dates, ctx->clock);
    mbedtls_ssl_set_bio(&ctx->ssl, ctx->socket, bio_send, bio_recv, NULL);

    TickType_t start = xTaskGetTickCount();
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            break;
        }
        if (xTaskGetTickCount() - start >= handshake_timeout) {
            break;
        }
    }
    if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
        log_refused(ctx, host);
        goto free_tls;
    } else if (ret != 0) {
        log_tls_error("TLS handshake failed with", host, ret);
        goto free_tls;
    }

    set_timeouts(ctx->socket, seconds_to_ticks(ctx->to_recv),
            seconds_to_ticks(ctx->to_send));
    return KHC_SOCK_OK;

free_tls:
    mbedtls_ssl_free(&ctx->ssl);
close_socket:
    FreeRTOS_closesocket(ctx->socket);
    ctx->socket = FREERTOS_INVALID_SOCKET;
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_send(void* sock_ctx,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    int ret = mbedtls_ssl_write(&ctx->ssl, (const unsigned char*)buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return KHC_SOCK_OK;
    }
    /* Failed, or timed out. */
    sample_log("failed to send: -0x%04x", (unsigned int)-ret);
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_recv(void* sock_ctx, char* buffer, size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_actual_length = 0;
    int ret = mbedtls_ssl_read(&ctx->ssl, (unsigned char*)buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return KHC_SOCK_OK;
    }
    ctx->recv_ret = ret;
    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            ret == MBEDTLS_ERR_SSL_CONN_EOF) {
        /* The server closed the connection. */
        return KHC_SOCK_OK;
    }
    return KHC_SOCK_FAIL;
}

/* sock_cb_recv() returns KHC_SOCK_FAIL both when the receive timeout set in
   sock_cb_connect() runs out and when the connection fails. The timeout ends
   FreeRTOS_recv() with nothing read, which bio_recv() tells mbedTLS is wanting
   to read. */
kii_bool_t
    sock_cb_recv_timed_out(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    return ctx->recv_ret == MBEDTLS_ERR_SSL_WANT_READ ? KII_TRUE : KII_FALSE;
}

khc_sock_code_t
    sock_cb_close(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    if (ctx->socket == FREERTOS_INVALID_SOCKET) {
        return KHC_SOCK_OK;
    }
    /* The server may have closed the connection already. */
    (void)mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_ssl_free(&ctx->ssl);

    /* Closed gracefully, as FreeRTOS-Plus-TCP has it: once the server has
       closed its side too, which FreeRTOS_recv() tells with an error, or the
       wait is over. */
    FreeRTOS_shutdown(ctx->socket, FREERTOS_SHUT_RDWR);
    const TickType_t wait = pdMS_TO_TICKS(SHUTDOWN_TIMEOUT_MS);
    set_timeouts(ctx->socket, wait, wait);
    TickType_t start = xTaskGetTickCount();
    char discarded[64];
    while (FreeRTOS_recv(ctx->socket, discarded, sizeof(discarded), 0) >= 0 &&
            xTaskGetTickCount() - start < wait) {
    }
    FreeRTOS_closesocket(ctx->socket);
    ctx->socket = FREERTOS_INVALID_SOCKET;
    return KHC_SOCK_OK;
}
