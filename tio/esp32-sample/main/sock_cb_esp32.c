#include "sock_cb_esp32.h"

#include <string.h>
#include <sys/socket.h>

#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <khc.h>

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

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send,
        kii_clock_t* clock)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
    ctx->clock = clock;
}

static int set_timeout(int fd, int option, unsigned int seconds)
{
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return setsockopt(fd, SOL_SOCKET, option, &tv, sizeof(tv));
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
static kii_validity_t check_chain(kii_clock_t* clock, const mbedtls_x509_crt* chain)
{
    const mbedtls_x509_crt* crt = chain;
    /* As deep as mbedTLS verifies by default. */
    for (int depth = 0; crt != NULL && depth <= MBEDTLS_X509_MAX_INTERMEDIATE_CA + 1; ++depth) {
        kii_validity_t validity = check_dates(clock, crt);
        if (validity != KII_VALIDITY_OK) {
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
    return KII_VALIDITY_OK;
}

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            uint16_t port)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    ctx->tls = esp_tls_init();
    if (ctx->tls == NULL) {
        ESP_LOGE(TAG, "failed to allocate the TLS context");
        return KHC_SOCK_FAIL;
    }

    /* The server's certificate has to chain to one of the CA certificates
       ESP-IDF bundles, and be issued for host. */
    esp_tls_cfg_t cfg = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HANDSHAKE_TIMEOUT_MS,
    };
    if (esp_tls_conn_new_sync(host, strlen(host), port, &cfg, ctx->tls) != 1) {
        ESP_LOGE(TAG, "failed to connect to %s:%u", host, port);
        goto fail;
    }

    const mbedtls_ssl_context* ssl = esp_tls_get_ssl_context(ctx->tls);
    const mbedtls_x509_crt* chain = ssl == NULL ? NULL : mbedtls_ssl_get_peer_cert(ssl);
    if (chain == NULL) {
        ESP_LOGE(TAG, "no certificate from %s to check", host);
        goto fail;
    }
    switch (check_chain(ctx->clock, chain)) {
        case KII_VALIDITY_OK:
            break;
        case KII_VALIDITY_NOT_YET_VALID:
            ESP_LOGE(TAG, "server certificate for %s refused: not valid yet", host);
            kii_clock_forget_time(ctx->clock);
            goto fail;
        default:
            ESP_LOGE(TAG, "server certificate for %s refused: expired", host);
            goto fail;
    }

    int fd = -1;
    if (esp_tls_get_conn_sockfd(ctx->tls, &fd) != ESP_OK ||
            set_timeout(fd, SO_RCVTIMEO, ctx->to_recv) != 0 ||
            set_timeout(fd, SO_SNDTIMEO, ctx->to_send) != 0) {
        ESP_LOGE(TAG, "failed to set the timeouts");
        goto fail;
    }
    return KHC_SOCK_OK;

fail:
    esp_tls_conn_destroy(ctx->tls);
    ctx->tls = NULL;
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_send(void* sock_ctx,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    ssize_t ret = esp_tls_conn_write(ctx->tls, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return KHC_SOCK_OK;
    }
    /* Failed, or timed out. */
    ESP_LOGE(TAG, "failed to send: -0x%x", (unsigned int)-ret);
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_recv(void* sock_ctx, char* buffer, size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    *out_actual_length = 0;
    ssize_t ret = esp_tls_conn_read(ctx->tls, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return KHC_SOCK_OK;
    }
    ctx->recv_ret = ret;
    if (ret == 0) {
        /* The server closed the connection. */
        return KHC_SOCK_OK;
    }
    return KHC_SOCK_FAIL;
}

/* sock_cb_recv() returns KHC_SOCK_FAIL both when the receive timeout set in
   sock_cb_connect() runs out and when the connection fails. The timeout makes
   lwIP's read fail with EAGAIN, which mbedTLS reports as wanting to read. */
kii_bool_t
    sock_cb_recv_timed_out(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    return ctx->recv_ret == ESP_TLS_ERR_SSL_WANT_READ ? KII_TRUE : KII_FALSE;
}

khc_sock_code_t
    sock_cb_close(void* sock_ctx)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    int ret = esp_tls_conn_destroy(ctx->tls);
    ctx->tls = NULL;
    return ret == 0 ? KHC_SOCK_OK : KHC_SOCK_FAIL;
}
