#include "ebisu_mbedtls.h"

#include <ebisu_object.h>
#include <ebisu_time.h>

#include <mbedtls/build_info.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#if MBEDTLS_VERSION_MAJOR < 4
#include <mbedtls/psa_util.h>
#endif

#include <string.h>

#if MBEDTLS_VERSION_MAJOR < 3 || (MBEDTLS_VERSION_MAJOR == 3 && MBEDTLS_VERSION_MINOR < 6)
#error "ebisu_mbedtls needs mbedTLS 3.6 or later."
#endif

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#define log_tls_error(logger, what, host, ret) ((void)0)
#define log_refused(logger, host, flags) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)

/* What failed, with mbedTLS's error and, built with MBEDTLS_ERROR_C, what it
   says it is. */
static void log_tls_error(ebisu_logger_t* logger, const char* what, const char* host, int ret)
{
    char description[96] = "";
#if defined(MBEDTLS_ERROR_C)
    mbedtls_strerror(ret, description, sizeof(description));
#endif
    log_to(logger, EBISU_LOG_ERROR, "%s %s: -0x%04x %s", what, host, (unsigned int)-ret,
            description);
}

/* Why the server's certificate was refused: the first of the reasons mbedTLS
   gives, one a line. */
static void log_refused(ebisu_logger_t* logger, const char* host, uint32_t flags)
{
    char reason[128];
    if (mbedtls_x509_crt_verify_info(reason, sizeof(reason), "", flags) < 0) {
        reason[0] = '\0';
    }
    reason[strcspn(reason, "\n")] = '\0';
    log_to(logger, EBISU_LOG_ERROR, "server certificate for %s refused: %s", host, reason);
}
#endif

int ebisu_mbedtls_tls_init(ebisu_mbedtls_tls_t* tls, const unsigned char* ca, size_t ca_size)
{
    int ret;
    mbedtls_ssl_config_init(&tls->_config);
    mbedtls_x509_crt_init(&tls->_ca);
    /* Before parsing a certificate, which needs PSA in mbedTLS 4. */
    ret = (int)psa_crypto_init();
    if (ret != 0) {
        goto fail;
    }
    ret = mbedtls_x509_crt_parse(&tls->_ca, ca, ca_size);
    if (ret > 0) {
        /* How many of the certificates it couldn't parse. */
        ret = MBEDTLS_ERR_X509_CERT_UNKNOWN_FORMAT;
    }
    if (ret != 0) {
        goto fail;
    }
    ret = mbedtls_ssl_config_defaults(&tls->_config, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        goto fail;
    }
    mbedtls_ssl_conf_min_tls_version(&tls->_config, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_authmode(&tls->_config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->_config, &tls->_ca, NULL);
#if MBEDTLS_VERSION_MAJOR < 4
    /* PSA's, as mbedTLS 4 always takes. */
    mbedtls_ssl_conf_rng(&tls->_config, mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
#endif
    return 0;

fail:
    ebisu_mbedtls_tls_free(tls);
    return ret;
}

mbedtls_ssl_config* ebisu_mbedtls_tls_config(ebisu_mbedtls_tls_t* tls)
{
    return &tls->_config;
}

void ebisu_mbedtls_tls_free(ebisu_mbedtls_tls_t* tls)
{
    mbedtls_ssl_config_free(&tls->_config);
    mbedtls_x509_crt_free(&tls->_ca);
}

static void set_error(ebisu_mbedtls_socket_t* sock, ebisu_sock_error_t error, long detail)
{
    sock->_error = error;
    sock->_error_detail = detail;
}

/* What the socket under it said of the failure it just had, if it says. */
static void take_transport_error(ebisu_mbedtls_socket_t* sock)
{
    ebisu_socket_t* transport = sock->_transport;
    sock->_transport_error = EBISU_SOCK_ERROR_NONE;
    sock->_transport_detail = 0;
    if (transport->ops->last_error != NULL) {
        sock->_transport_error =
            transport->ops->last_error(transport, &sock->_transport_detail);
    }
}

/* mbedTLS's input and output, on the socket under it. A timeout of its ends a
   send or a receive with nothing done, which mbedTLS is told is wanting to
   write or read, as it is for EBISU_SOCK_AGAIN, of a socket that doesn't
   block: _timed_out and _again tell them apart, and from mbedTLS's wanting to
   be called again with neither, having taken a record with nothing for the
   caller, as mbedTLS 4 does with a TLS 1.3 server's session ticket. */
static int bio_send(void* arg, const unsigned char* buffer, size_t length)
{
    ebisu_mbedtls_socket_t* sock = (ebisu_mbedtls_socket_t*)arg;
    ebisu_socket_t* transport = sock->_transport;
    size_t sent = 0;
    switch (transport->ops->send(transport, (const char*)buffer, length, &sent)) {
        case EBISU_SOCK_OK:
            if (sent > 0) {
                return (int)sent;
            }
            sock->_again = true;
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        case EBISU_SOCK_AGAIN:
            sock->_again = true;
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        default:
            take_transport_error(sock);
            if (sock->_transport_error == EBISU_SOCK_ERROR_TIMEOUT) {
                sock->_timed_out = true;
                return MBEDTLS_ERR_SSL_WANT_WRITE;
            }
            return MBEDTLS_ERR_NET_SEND_FAILED;
    }
}

static int bio_recv(void* arg, unsigned char* buffer, size_t length)
{
    ebisu_mbedtls_socket_t* sock = (ebisu_mbedtls_socket_t*)arg;
    ebisu_socket_t* transport = sock->_transport;
    size_t received = 0;
    switch (transport->ops->recv(transport, (char*)buffer, length, &received)) {
        case EBISU_SOCK_OK:
            /* 0, the end of the stream, as mbedTLS takes it. */
            return (int)received;
        case EBISU_SOCK_AGAIN:
            sock->_again = true;
            return MBEDTLS_ERR_SSL_WANT_READ;
        default: {
            bool timed_out;
            take_transport_error(sock);
            timed_out = transport->ops->recv_timed_out != NULL ?
                transport->ops->recv_timed_out(transport) :
                sock->_transport_error == EBISU_SOCK_ERROR_TIMEOUT;
            if (timed_out) {
                sock->_timed_out = true;
                return MBEDTLS_ERR_SSL_WANT_READ;
            }
            return MBEDTLS_ERR_NET_RECV_FAILED;
        }
    }
}

/* The validity period of a certificate, at the clock's time. */
static ebisu_validity_t check_dates(ebisu_clock_t* clock, const mbedtls_x509_crt* crt)
{
    const mbedtls_x509_time* from = &crt->valid_from;
    const mbedtls_x509_time* to = &crt->valid_to;
    int64_t not_before = 0;
    int64_t not_after = 0;
    if (!ebisu_time_from_civil(from->year, from->mon, from->day, from->hour, from->min,
                from->sec, &not_before) ||
            !ebisu_time_from_civil(to->year, to->mon, to->day, to->hour, to->min, to->sec,
                &not_after)) {
        /* A date that isn't one. */
        return EBISU_VALIDITY_EXPIRED;
    }
    return ebisu_clock_check_validity(clock, not_before, not_after);
}

/* mbedTLS calls this for each certificate of the chain it built, from the CA's
   to the server's, with what it found wrong with it in flags. With a clock,
   the validity periods are the clock's to check, in place of mbedTLS's check
   at the system's time, if it made one. */
static int verify(void* arg, mbedtls_x509_crt* crt, int depth, uint32_t* flags)
{
    ebisu_mbedtls_socket_t* sock = (ebisu_mbedtls_socket_t*)arg;
    (void)depth;
    if (sock->_clock == NULL) {
        return 0;
    }
    *flags &= ~(uint32_t)(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE);
    switch (check_dates(sock->_clock, crt)) {
        case EBISU_VALIDITY_OK:
            break;
        case EBISU_VALIDITY_NOT_YET_VALID:
            *flags |= MBEDTLS_X509_BADCERT_FUTURE;
            break;
        default:
            *flags |= MBEDTLS_X509_BADCERT_EXPIRED;
            break;
    }
    return 0;
}

/* Why an operation failed, from mbedTLS's error, and what the socket under it
   said, for what failed there. */
static void set_tls_error(ebisu_mbedtls_socket_t* sock, int ret)
{
    switch (ret) {
        case MBEDTLS_ERR_SSL_WANT_READ:
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            set_error(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
            break;
        case MBEDTLS_ERR_SSL_CONN_EOF:
        case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
            set_error(sock, EBISU_SOCK_ERROR_CLOSED, 0);
            break;
        case MBEDTLS_ERR_NET_SEND_FAILED:
        case MBEDTLS_ERR_NET_RECV_FAILED:
            set_error(sock, sock->_transport_error != EBISU_SOCK_ERROR_NONE ?
                    sock->_transport_error : EBISU_SOCK_ERROR_OTHER, sock->_transport_detail);
            break;
        default:
            set_error(sock, EBISU_SOCK_ERROR_TLS, ret);
            break;
    }
}

/* Whether mbedTLS's answer is that it wants to be called again, at once, with
   neither a timeout nor EBISU_SOCK_AGAIN of the socket under it. */
static bool call_again(const ebisu_mbedtls_socket_t* sock, int ret)
{
    return (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) &&
        !sock->_timed_out && !sock->_again;
}

/* Done with the connection: with a close_notify, unless mbedTLS ended it. */
static void end(ebisu_mbedtls_socket_t* sock, bool notify)
{
    if (sock->_open) {
        if (notify && !sock->_failed) {
            /* The server may have gone already. */
            (void)mbedtls_ssl_close_notify(&sock->_ssl);
        }
        mbedtls_ssl_free(&sock->_ssl);
        sock->_open = false;
    }
}

static ebisu_sock_code_t mbedtls_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    ebisu_socket_t* transport = sock->_transport;
    int ret;
    set_error(sock, EBISU_SOCK_ERROR_NONE, 0);
    sock->_failed = false;
    sock->_recv_timed_out = false;
#if !defined(MBEDTLS_HAVE_TIME_DATE)
    if (sock->_clock == NULL) {
        /* Nothing to check the certificates' dates at. */
        log_to(sock->_logger, EBISU_LOG_ERROR,
                "no clock to check the certificate of %s at, and no time of the system's",
                host);
        set_error(sock, EBISU_SOCK_ERROR_TLS, 0);
        return EBISU_SOCK_FAIL;
    }
#endif
    if (transport->ops->connect(transport, host, port) != EBISU_SOCK_OK) {
        take_transport_error(sock);
        set_error(sock, sock->_transport_error != EBISU_SOCK_ERROR_NONE ?
                sock->_transport_error : EBISU_SOCK_ERROR_CONNECT, sock->_transport_detail);
        return EBISU_SOCK_FAIL;
    }

    mbedtls_ssl_init(&sock->_ssl);
    sock->_open = true;
    ret = mbedtls_ssl_setup(&sock->_ssl, &sock->_tls->_config);
    if (ret == 0) {
        ret = mbedtls_ssl_set_hostname(&sock->_ssl, host);
    }
    if (ret != 0) {
        log_tls_error(sock->_logger, "failed to set up TLS for", host, ret);
        set_error(sock, EBISU_SOCK_ERROR_TLS, ret);
        goto fail;
    }
    if (sock->_clock != NULL) {
        int64_t now = 0;
        sock->_time_exact = ebisu_clock_get_time(sock->_clock, &now);
    }
    mbedtls_ssl_set_verify(&sock->_ssl, verify, sock);
    mbedtls_ssl_set_bio(&sock->_ssl, sock, bio_send, bio_recv, NULL);

    for (;;) {
        sock->_timed_out = false;
        sock->_again = false;
        ret = mbedtls_ssl_handshake(&sock->_ssl);
        /* Not timed out: called again, as for a socket that doesn't block,
           which connect waits for. */
        if ((ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) &&
                !sock->_timed_out) {
            continue;
        }
        break;
    }
    if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
        uint32_t flags = mbedtls_ssl_get_verify_result(&sock->_ssl);
        /* Not valid yet at the time the clock had: more likely than a
           certificate from the future, the time fell behind. Checked at the
           clock's floor, the next connection goes through, and its response
           gives the clock the time again. */
        if ((flags & MBEDTLS_X509_BADCERT_FUTURE) != 0 && sock->_clock != NULL &&
                sock->_time_exact) {
            ebisu_clock_forget_time(sock->_clock);
        }
        log_refused(sock->_logger, host, flags);
        set_error(sock, EBISU_SOCK_ERROR_VERIFY, (long)flags);
        goto fail;
    } else if (ret != 0) {
        log_tls_error(sock->_logger, "TLS handshake failed with", host, ret);
        set_tls_error(sock, ret);
        goto fail;
    }
    return EBISU_SOCK_OK;

fail:
    /* No close_notify for a handshake that didn't end. */
    end(sock, false);
    (void)transport->ops->close(transport);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t mbedtls_send(
        ebisu_socket_t* self,
        const char* buffer,
        size_t length,
        size_t* out_sent_length)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    int ret;
    *out_sent_length = 0;
    do {
        sock->_timed_out = false;
        sock->_again = false;
        ret = mbedtls_ssl_write(&sock->_ssl, (const unsigned char*)buffer, length);
    } while (call_again(sock, ret));
    if (ret > 0) {
        *out_sent_length = (size_t)ret;
        return EBISU_SOCK_OK;
    }
    if (sock->_again) {
        return EBISU_SOCK_AGAIN;
    }
    if (!sock->_timed_out) {
        sock->_failed = true;
        log_tls_error(sock->_logger, "failed to send to", "the server", ret);
    }
    set_tls_error(sock, ret);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t mbedtls_recv(
        ebisu_socket_t* self,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    int ret;
    *out_actual_length = 0;
    sock->_recv_timed_out = false;
    for (;;) {
        sock->_timed_out = false;
        sock->_again = false;
        ret = mbedtls_ssl_read(&sock->_ssl, (unsigned char*)buffer, length_to_read);
#if defined(MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET)
        /* A TLS 1.3 server's ticket, after the handshake, which mbedTLS 3.6
           returns, and the socket doesn't resume sessions with. */
        if (ret == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            continue;
        }
#endif
        if (call_again(sock, ret)) {
            continue;
        }
        break;
    }
    if (ret > 0) {
        *out_actual_length = (size_t)ret;
        return EBISU_SOCK_OK;
    }
    if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == MBEDTLS_ERR_SSL_CONN_EOF) {
        /* The end of the stream. */
        return EBISU_SOCK_OK;
    }
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if (sock->_again) {
            return EBISU_SOCK_AGAIN;
        }
        /* Timed out: routine on an idle connection, which goes on as it
           was. */
        sock->_recv_timed_out = true;
        set_error(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
        return EBISU_SOCK_FAIL;
    }
    sock->_failed = true;
    log_tls_error(sock->_logger, "failed to receive from", "the server", ret);
    set_tls_error(sock, ret);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t mbedtls_close(ebisu_socket_t* self)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    if (!sock->_open) {
        return EBISU_SOCK_OK;
    }
    end(sock, true);
    return sock->_transport->ops->close(sock->_transport);
}

static bool mbedtls_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    return sock->_recv_timed_out;
}

static ebisu_sock_error_t mbedtls_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    *out_detail = sock->_error_detail;
    return sock->_error;
}

/* What mbedTLS has decrypted and recv hasn't returned, or has of a record, is
   for recv to return without waiting. */
static bool mbedtls_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_mbedtls_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_mbedtls_socket_t, base);
    if (mbedtls_ssl_get_bytes_avail(&sock->_ssl) > 0 || mbedtls_ssl_check_pending(&sock->_ssl)) {
        return true;
    }
    return sock->_transport->ops->recv_wait(sock->_transport, timeout_ms);
}

/* With recv_wait when the socket under it has one, and without when it hasn't,
   for the MQTT task to wait in recv instead. */
static const ebisu_socket_ops_t ops_waiting = {
    mbedtls_connect,
    mbedtls_send,
    mbedtls_recv,
    mbedtls_close,
    mbedtls_recv_timed_out,
    mbedtls_last_error,
    mbedtls_recv_wait
};

static const ebisu_socket_ops_t ops = {
    mbedtls_connect,
    mbedtls_send,
    mbedtls_recv,
    mbedtls_close,
    mbedtls_recv_timed_out,
    mbedtls_last_error,
    NULL
};

void ebisu_mbedtls_socket_init(
        ebisu_mbedtls_socket_t* sock,
        ebisu_socket_t* transport,
        ebisu_mbedtls_tls_t* tls)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = transport->ops->recv_wait != NULL ? &ops_waiting : &ops;
    sock->_transport = transport;
    sock->_tls = tls;
}

void ebisu_mbedtls_socket_set_clock(ebisu_mbedtls_socket_t* sock, ebisu_clock_t* clock)
{
    sock->_clock = clock;
}

void ebisu_mbedtls_socket_set_logger(ebisu_mbedtls_socket_t* sock, ebisu_logger_t* logger)
{
    sock->_logger = logger;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
