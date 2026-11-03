#include "sock_cb_esp32.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <ebisu_time.h>

#if !defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
#error "The certificates' dates are checked after the handshake, from the certificates it received: set CONFIG_MBEDTLS_SSL_KEEP_PEER_CERTIFICATE."
#endif
#if defined(MBEDTLS_HAVE_TIME_DATE)
#error "mbedTLS would check the certificates' dates at the system's time, 1970 on a device that has not set it, and refuse them all: unset CONFIG_MBEDTLS_HAVE_TIME_DATE. They are checked at the clock's time here."
#endif

static const char* TAG = "sock_cb";

/* For the handshake, which takes a few seconds on the device and more under
   QEMU. The connection's own timeouts are set after it. */
#define HANDSHAKE_TIMEOUT_MS 30000

static const ebisu_socket_ops_t sock_cb_ops = {
    sock_cb_connect,
    sock_cb_send,
    sock_cb_recv,
    sock_cb_close,
    sock_cb_recv_timed_out,
    sock_cb_last_error
};

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send,
        ebisu_clock_t* clock)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.ops = &sock_cb_ops;
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
    ctx->clock = clock;
}

/* Says why the operation failing now failed, for sock_cb_last_error(). */
static void set_error(socket_context_t* ctx, ebisu_sock_error_t error, long detail)
{
    ctx->error = error;
    ctx->error_detail = detail;
}

/* Why the handshake, a read or a write failed, from mbedTLS's error. */
static ebisu_sock_error_t tls_error(int ret)
{
    switch (ret) {
        case MBEDTLS_ERR_SSL_WANT_READ:
        case MBEDTLS_ERR_SSL_WANT_WRITE:
        case MBEDTLS_ERR_SSL_TIMEOUT:
            /* The socket's timeout. */
            return EBISU_SOCK_ERROR_TIMEOUT;
        case MBEDTLS_ERR_NET_CONN_RESET:
        case MBEDTLS_ERR_SSL_CONN_EOF:
        case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
            return EBISU_SOCK_ERROR_CLOSED;
        case MBEDTLS_ERR_NET_RECV_FAILED:
        case MBEDTLS_ERR_NET_SEND_FAILED:
            return EBISU_SOCK_ERROR_OTHER;
        default:
            return EBISU_SOCK_ERROR_TLS;
    }
}

/* Why esp_tls_conn_new_sync() failed, from what ESP-TLS kept of it: the step
   that failed, the socket's errno, and mbedTLS's error, negated, with the
   verify flags if the server's certificate was refused. */
static void set_connect_error(socket_context_t* ctx)
{
    esp_tls_error_handle_t handle = NULL;
    if (esp_tls_get_error_handle(ctx->tls, &handle) != ESP_OK) {
        set_error(ctx, EBISU_SOCK_ERROR_OTHER, 0);
        return;
    }
    int step = 0;
    int sock_errno = 0;
    int tls_code = 0;
    int flags = 0;
    esp_tls_get_and_clear_error_type(handle, ESP_TLS_ERR_TYPE_ESP, &step);
    esp_tls_get_and_clear_error_type(handle, ESP_TLS_ERR_TYPE_SYSTEM, &sock_errno);
    esp_tls_get_and_clear_error_type(handle, ESP_TLS_ERR_TYPE_MBEDTLS, &tls_code);
    esp_tls_get_and_clear_error_type(handle, ESP_TLS_ERR_TYPE_MBEDTLS_CERT_FLAGS, &flags);
    switch (step) {
        case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME:
            set_error(ctx, EBISU_SOCK_ERROR_DNS, 0);
            break;
        case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST:
            set_error(ctx, EBISU_SOCK_ERROR_CONNECT, sock_errno);
            break;
        case ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT:
            set_error(ctx, EBISU_SOCK_ERROR_TIMEOUT, 0);
            break;
        case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED:
            if (flags != 0) {
                set_error(ctx, EBISU_SOCK_ERROR_VERIFY, flags);
            } else {
                set_error(ctx, tls_error(-tls_code), -tls_code);
            }
            break;
        default:
            set_error(ctx, EBISU_SOCK_ERROR_OTHER, step);
            break;
    }
}

static int set_timeout(int fd, int option, unsigned int seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv));
}

/* The validity period of a certificate, against the clock. */
static ebisu_validity_t check_dates(ebisu_clock_t* clock, const mbedtls_x509_crt* crt)
{
    const mbedtls_x509_time* from = &crt->valid_from;
    const mbedtls_x509_time* to = &crt->valid_to;
    int64_t not_before = 0;
    int64_t not_after = 0;
    if (!ebisu_time_from_civil(from->year, from->mon, from->day,
                from->hour, from->min, from->sec, &not_before) ||
            !ebisu_time_from_civil(to->year, to->mon, to->day,
                to->hour, to->min, to->sec, &not_after)) {
        /* A date that is not one. */
        return EBISU_VALIDITY_EXPIRED;
    }
    return ebisu_clock_check_validity(clock, not_before, not_after);
}

static int same_name(const mbedtls_x509_buf* a, const mbedtls_x509_buf* b)
{
    return a->len == b->len && memcmp(a->p, b->p, a->len) == 0;
}

/* mbedTLS has verified the server's certificate: that it chains to a CA
   certificate in ESP-IDF's bundle, and that it is issued for the host. Not its
   dates, which mbedTLS would check at the system's time. They are checked here
   at the clock's: its floor until a server has stated the time, which only
   tells a certificate that has expired, and that time from then on.

   Of each certificate from the server's to the CA's, through those the
   handshake received: from each one, the next is the one it names as its
   issuer. A server may send others, an old cross-signed certificate for
   instance, which the chain mbedTLS verified does not go through.

   A certificate not valid yet at a time the clock had as exact is refused,
   and the time forgotten: more likely than a certificate from the future, the
   time fell behind, the monotonic clock having stopped while the device slept
   and the server having renewed its certificate meanwhile. Checked against a
   lower bound, the next connection goes through, and its response gives the
   clock the time again. */
static ebisu_validity_t check_chain(ebisu_clock_t* clock, const mbedtls_x509_crt* chain)
{
    const mbedtls_x509_crt* crt = chain;
    /* As deep as mbedTLS verifies by default. */
    for (int depth = 0; crt != NULL && depth <= MBEDTLS_X509_MAX_INTERMEDIATE_CA + 1; ++depth) {
        ebisu_validity_t validity = check_dates(clock, crt);
        if (validity != EBISU_VALIDITY_OK) {
            return validity;
        }
        if (same_name(&crt->issuer_raw, &crt->subject_raw)) {
            break;
        }
        const mbedtls_x509_crt* issuer = NULL;
        for (const mbedtls_x509_crt* c = chain; c != NULL; c = c->next) {
            if (c != crt && same_name(&c->subject_raw, &crt->issuer_raw)) {
                issuer = c;
                break;
            }
        }
        crt = issuer;
    }
    return EBISU_VALIDITY_OK;
}

ebisu_sock_code_t
    sock_cb_connect(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    ctx->tls = esp_tls_init();
    if (ctx->tls == NULL) {
        ESP_LOGE(TAG, "failed to allocate the TLS context");
        set_error(ctx, EBISU_SOCK_ERROR_OTHER, 0);
        return EBISU_SOCK_FAIL;
    }

    /* The server's certificate has to chain to one of the CA certificates
       ESP-IDF bundles, and be issued for host. */
    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HANDSHAKE_TIMEOUT_MS,
    };
    if (esp_tls_conn_new_sync(host, strlen(host), port, &cfg, ctx->tls) != 1) {
        ESP_LOGE(TAG, "failed to connect to %s:%u", host, port);
        set_connect_error(ctx);
        goto fail;
    }

    const mbedtls_ssl_context* ssl = esp_tls_get_ssl_context(ctx->tls);
    const mbedtls_x509_crt* chain = ssl == NULL ? NULL : mbedtls_ssl_get_peer_cert(ssl);
    if (chain == NULL) {
        ESP_LOGE(TAG, "no certificate from %s to check", host);
        set_error(ctx, EBISU_SOCK_ERROR_VERIFY, 0);
        goto fail;
    }
    switch (check_chain(ctx->clock, chain)) {
        case EBISU_VALIDITY_OK:
            break;
        case EBISU_VALIDITY_NOT_YET_VALID:
            ESP_LOGE(TAG, "server certificate for %s refused: not valid yet", host);
            ebisu_clock_forget_time(ctx->clock);
            set_error(ctx, EBISU_SOCK_ERROR_VERIFY, MBEDTLS_X509_BADCERT_FUTURE);
            goto fail;
        default:
            ESP_LOGE(TAG, "server certificate for %s refused: expired", host);
            set_error(ctx, EBISU_SOCK_ERROR_VERIFY, MBEDTLS_X509_BADCERT_EXPIRED);
            goto fail;
    }

    int fd = -1;
    if (esp_tls_get_conn_sockfd(ctx->tls, &fd) != ESP_OK ||
            set_timeout(fd, SO_RCVTIMEO, ctx->to_recv) != 0 ||
            set_timeout(fd, SO_SNDTIMEO, ctx->to_send) != 0) {
        set_error(ctx, EBISU_SOCK_ERROR_OTHER, errno);
        ESP_LOGE(TAG, "failed to set the timeouts");
        goto fail;
    }
    return EBISU_SOCK_OK;

fail:
    esp_tls_conn_destroy(ctx->tls);
    ctx->tls = NULL;
    return EBISU_SOCK_FAIL;
}

ebisu_sock_code_t
    sock_cb_send(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    ssize_t ret = esp_tls_conn_write(ctx->tls, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return EBISU_SOCK_OK;
    }
    /* Failed, or timed out. */
    ESP_LOGE(TAG, "failed to send: -0x%x", (unsigned int)-ret);
    set_error(ctx, tls_error(ret), ret);
    return EBISU_SOCK_FAIL;
}

ebisu_sock_code_t
    sock_cb_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    *out_actual_length = 0;
    ssize_t ret = esp_tls_conn_read(ctx->tls, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return EBISU_SOCK_OK;
    }
    ctx->recv_ret = ret;
    if (ret == 0) {
        /* The server closed the connection. */
        return EBISU_SOCK_OK;
    }
    set_error(ctx, tls_error(ret), ret);
    return EBISU_SOCK_FAIL;
}

/* sock_cb_recv() returns EBISU_SOCK_FAIL both when the receive timeout set in
   sock_cb_connect() runs out and when the connection fails. The timeout makes
   lwIP's read fail with EAGAIN, which mbedTLS reports as wanting to read. */
bool
    sock_cb_recv_timed_out(ebisu_socket_t* self)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    return ctx->recv_ret == ESP_TLS_ERR_SSL_WANT_READ;
}

/* Why the last operation that returned EBISU_SOCK_FAIL failed. The detail is
   mbedTLS's error, its verify flags, MBEDTLS_X509_BADCERT_*, for a certificate
   refused, or errno for an address that refused. */
ebisu_sock_error_t
    sock_cb_last_error(ebisu_socket_t* self, long* out_detail)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    *out_detail = ctx->error_detail;
    return ctx->error;
}

ebisu_sock_code_t
    sock_cb_close(ebisu_socket_t* self)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    int ret = esp_tls_conn_destroy(ctx->tls);
    ctx->tls = NULL;
    return ret == 0 ? EBISU_SOCK_OK : EBISU_SOCK_FAIL;
}
