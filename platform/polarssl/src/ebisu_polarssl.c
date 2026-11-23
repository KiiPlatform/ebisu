#include "ebisu_polarssl.h"

#include <ebisu_dns.h>
#include <ebisu_object.h>
#include <ebisu_time.h>

#include <polarssl/version.h>
#include <polarssl/net.h>
#include <polarssl/pk.h>
#include <polarssl/sha256.h>
#include <polarssl/ssl.h>
#include <polarssl/x509.h>
#include <polarssl/x509_crt.h>
#if defined(POLARSSL_ERROR_C)
#include <polarssl/error.h>
#endif

#include <string.h>

#if POLARSSL_VERSION_NUMBER < 0x01030800 || POLARSSL_VERSION_NUMBER >= 0x01040000
#error "ebisu_polarssl needs PolarSSL 1.3, from 1.3.8, or mbedTLS 1.3."
#endif
#if !defined(POLARSSL_SSL_CLI_C) || !defined(POLARSSL_SSL_PROTO_TLS1_2) || \
    !defined(POLARSSL_SHA256_C)
#error "ebisu_polarssl is a client of TLS 1.2: set POLARSSL_SSL_CLI_C, POLARSSL_SSL_PROTO_TLS1_2 and POLARSSL_SHA256_C."
#endif

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#define log_tls_error(logger, what, host, ssl, ret) ((void)0)
#define log_refused(logger, host, flags) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)

/* What failed, with PolarSSL's error and, built with POLARSSL_ERROR_C, what it
   says it is. A record PolarSSL refused that is longer than it takes is said
   too: a build that takes less than the 16384 bytes a server may send in a
   record, as embedded ones are, otherwise fails as for a record that isn't
   TLS, or, just over, within the room the buffer has for padding and MAC, as
   for a MAC that doesn't match, with AES-CBC. Its length is in the header,
   which PolarSSL leaves in the context's in_hdr. */
static void log_tls_error(ebisu_logger_t* logger, const char* what, const char* host,
        const ssl_context* ssl, int ret)
{
    char description[96] = "";
#if defined(POLARSSL_ERROR_C)
    polarssl_strerror(ret, description, sizeof(description));
#endif
    log_to(logger, EBISU_LOG_ERROR, "%s %s: -0x%04x %s", what, host, (unsigned int)-ret,
            description);
    if ((ret != POLARSSL_ERR_SSL_INVALID_RECORD && ret != POLARSSL_ERR_SSL_INVALID_MAC) ||
            ssl->in_hdr == NULL) {
        return;
    }
    unsigned int length = (unsigned int)ssl->in_hdr[3] << 8 | ssl->in_hdr[4];
    /* Of one of TLS's types, and its version 3: not, say, the start of an
       HTTP response, which PolarSSL refuses as a record too. */
    if (ssl->in_hdr[0] >= SSL_MSG_CHANGE_CIPHER_SPEC &&
            ssl->in_hdr[0] <= SSL_MSG_APPLICATION_DATA && ssl->in_hdr[1] == 3 &&
            length > SSL_MAX_CONTENT_LEN) {
        log_to(logger, EBISU_LOG_ERROR,
                "%s sent a record of %u bytes, more than the %u this PolarSSL takes, as "
                "SSL_MAX_CONTENT_LEN sets it. A server may send up to 16384 bytes of data "
                "in a record, unless it honours the max_fragment_length asked of it.",
                host, length, (unsigned int)SSL_MAX_CONTENT_LEN);
    }
}

/* Why the server's certificate was refused: the first of the reasons in the
   verify flags, as mbedTLS's mbedtls_x509_crt_verify_info() says them, which
   PolarSSL has none of. */
static void log_refused(ebisu_logger_t* logger, const char* host, int flags)
{
    static const struct {
        int flag;
        const char* reason;
    } reasons[] = {
        { BADCERT_EXPIRED, "The certificate validity has expired" },
        { BADCERT_REVOKED, "The certificate has been revoked (is on a CRL)" },
        { BADCERT_CN_MISMATCH, "The certificate Common Name (CN) does not match with the expected CN" },
        { BADCERT_NOT_TRUSTED, "The certificate is not correctly signed by the trusted CA" },
        { BADCERT_MISSING, "Certificate was missing" },
        { BADCERT_SKIP_VERIFY, "Certificate verification was skipped" },
        { BADCERT_OTHER, "Other reason (can be used by verify callback)" },
        { BADCERT_FUTURE, "The certificate validity starts in the future" }
    };
    const char* reason = "";
    size_t i;
    for (i = 0; i < sizeof(reasons) / sizeof(reasons[0]); ++i) {
        if ((flags & reasons[i].flag) != 0) {
            reason = reasons[i].reason;
            break;
        }
    }
    log_to(logger, EBISU_LOG_ERROR, "server certificate for %s refused: %s", host, reason);
}
#endif

#if EBISU_POLARSSL_SETS_RNG
/* The configuration's CTR_DRBG, for each connection, under its lock: PolarSSL
   1.3's has none of its own. */
static int drbg_random(void* arg, unsigned char* output, size_t length)
{
    ebisu_polarssl_tls_t* tls = (ebisu_polarssl_tls_t*)arg;
#if defined(POLARSSL_THREADING_C)
    int ret;
    if (polarssl_mutex_lock(&tls->_drbg_lock) != 0) {
        return POLARSSL_ERR_THREADING_MUTEX_ERROR;
    }
    ret = ctr_drbg_random(&tls->_drbg, output, length);
    if (polarssl_mutex_unlock(&tls->_drbg_lock) != 0) {
        return POLARSSL_ERR_THREADING_MUTEX_ERROR;
    }
    return ret;
#else
    return ctr_drbg_random(&tls->_drbg, output, length);
#endif
}
#endif

/* What tls_init() set up, the CTR_DRBG's lock with locked. */
static void free_parts(ebisu_polarssl_tls_t* tls, bool locked)
{
    x509_crt_free(&tls->_ca);
#if EBISU_POLARSSL_SETS_RNG
    ctr_drbg_free(&tls->_drbg);
    entropy_free(&tls->_entropy);
#if defined(POLARSSL_THREADING_C)
    if (locked) {
        polarssl_mutex_free(&tls->_drbg_lock);
    }
#endif
#endif
    (void)locked;
}

/* The cipher suites a configuration offers, by the numbers TLS gives them,
   which every version of PolarSSL shares, the most wanted first. Those with
   forward secrecy and authenticated encryption: TLS 1.3's, and TLS 1.2's with
   an ECDHE or DHE key exchange and AES-GCM, ChaCha20-Poly1305 or AES-CCM. */
static const uint16_t STRICT_SUITES[] = {
    0x1301, 0x1302, 0x1303,
    0xC02B, 0xC02F, 0xC02C, 0xC030, 0xCCA9, 0xCCA8,
    0x009E, 0x009F, 0xCCAA,
    0xC0AC, 0xC0AD, 0xC09E, 0xC09F,
};
/* Those of a PolarSSL built with none of the above, as some of vendors' SDKs
   build it: AES alone, in CBC or with an RSA key exchange. Neither list has
   RC4, 3DES, Camellia or ARIA, a NULL cipher, or a pre-shared key. */
static const uint16_t LEGACY_SUITES[] = {
    0xC023, 0xC027, 0xC024, 0xC028, 0xC009, 0xC013, 0xC00A, 0xC014,
    0x0067, 0x006B, 0x0033, 0x0039,
    0x009C, 0x009D, 0x003C, 0x003D, 0x002F, 0x0035,
};
#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* Those of wanted that PolarSSL is built with, in wanted's order, ended by a
   0, as PolarSSL takes a list. How many. */
static size_t pick_suites(const uint16_t* wanted, size_t count, int* out)
{
    size_t picked = 0;
    for (size_t i = 0; i < count; ++i) {
        for (const int* has = ssl_list_ciphersuites(); *has != 0; ++has) {
            if (*has == wanted[i]) {
                out[picked++] = wanted[i];
                break;
            }
        }
    }
    out[picked] = 0;
    return picked;
}

/* 0, or PolarSSL's error, the configuration then holding nothing to free. */
static int tls_init(ebisu_polarssl_tls_t* tls, const unsigned char* ca, size_t ca_size)
{
    int ret = 0;
    memset(tls, 0, sizeof(*tls));
    x509_crt_init(&tls->_ca);
    tls->_verify = EBISU_TLS_VERIFY_FULL;
#if EBISU_POLARSSL_SETS_RNG
    entropy_init(&tls->_entropy);
#if defined(POLARSSL_THREADING_C)
    ret = polarssl_mutex_init(&tls->_drbg_lock);
    if (ret != 0) {
        free_parts(tls, false);
        return ret;
    }
#endif
    {
        static const unsigned char personal[] = "ebisu_polarssl";
        /* PolarSSL 1.3's sets the context up and seeds it. */
        ret = ctr_drbg_init(&tls->_drbg, entropy_func, &tls->_entropy, personal,
                sizeof(personal) - 1);
    }
    if (ret != 0) {
        goto fail;
    }
#endif
    /* None, for a pin or no check: an empty list, which PolarSSL takes for
       one, and in which no certificate chains. */
    ret = ca == NULL && ca_size == 0 ? 0 : x509_crt_parse(&tls->_ca, ca, ca_size);
    if (ret > 0) {
        /* How many of the certificates it couldn't parse. */
        ret = POLARSSL_ERR_X509_CERT_UNKNOWN_FORMAT;
    }
    if (ret != 0) {
        goto fail;
    }
    /* None, and PolarSSL's own left, only if it is built with none of either. */
    if (pick_suites(STRICT_SUITES, COUNT_OF(STRICT_SUITES), tls->_suites) == 0) {
        pick_suites(LEGACY_SUITES, COUNT_OF(LEGACY_SUITES), tls->_suites);
    }
    return 0;

fail:
    free_parts(tls, true);
    return ret;
}

bool ebisu_polarssl_tls_init(ebisu_polarssl_tls_t* tls, const unsigned char* ca, size_t ca_size,
        int* out_error)
{
    int ret = tls_init(tls, ca, ca_size);
    if (out_error != NULL) {
        *out_error = ret;
    }
    return ret == 0;
}

void ebisu_polarssl_tls_set_setup(ebisu_polarssl_tls_t* tls, EBISU_POLARSSL_SETUP setup,
        void* userdata)
{
    tls->_setup = setup;
    tls->_setup_userdata = userdata;
}

bool ebisu_polarssl_tls_set_verify(ebisu_polarssl_tls_t* tls, ebisu_tls_verify_t verify,
        const unsigned char* pin)
{
    switch (verify) {
        case EBISU_TLS_VERIFY_FULL:
        case EBISU_TLS_VERIFY_CHAIN:
        case EBISU_TLS_VERIFY_NONE:
            memset(tls->_pin, 0, sizeof(tls->_pin));
            break;
        case EBISU_TLS_VERIFY_PIN:
            if (pin == NULL) {
                return false;
            }
            memcpy(tls->_pin, pin, sizeof(tls->_pin));
            break;
        default:
            return false;
    }
    tls->_verify = verify;
    return true;
}

void ebisu_polarssl_tls_free(ebisu_polarssl_tls_t* tls)
{
    free_parts(tls, true);
}

static void set_error(ebisu_polarssl_socket_t* sock, ebisu_sock_error_t error, long detail)
{
    sock->_error = error;
    sock->_error_detail = detail;
}

/* What the socket under it said of the failure it just had, if it says. */
static void take_transport_error(ebisu_polarssl_socket_t* sock)
{
    ebisu_socket_t* transport = sock->_transport;
    sock->_transport_error = EBISU_SOCK_ERROR_NONE;
    sock->_transport_detail = 0;
    if (transport->ops->last_error != NULL) {
        sock->_transport_error =
            transport->ops->last_error(transport, &sock->_transport_detail);
    }
}

/* PolarSSL's input and output, on the socket under it. A timeout of its ends a
   send or a receive with nothing done, which PolarSSL is told is wanting to
   write or read, as it is for EBISU_SOCK_AGAIN, of a socket that doesn't
   block: _timed_out and _again tell them apart, and from PolarSSL's wanting
   to be called again with neither, having taken a record with nothing for the
   caller. */
/* Whether the handshake under way has a time it may take, set by
   ebisu_polarssl_socket_set_handshake_timeout(), and how much is left of it, 0
   once it has run out. The time gone by as a difference, never a deadline
   added to now. */
static bool handshake_bounded(const ebisu_polarssl_socket_t* sock, uint32_t* out_left_ms)
{
    if (!sock->_handshaking || sock->_handshake_ms == 0) {
        return false;
    }
    uint64_t gone_ms = ebisu_system_monotonic_ms(sock->_system) - sock->_handshake_start_ms;
    *out_left_ms = gone_ms >= sock->_handshake_ms ? 0 : (uint32_t)(sock->_handshake_ms - gone_ms);
    return true;
}

static int bio_send(void* arg, const unsigned char* buffer, size_t length)
{
    ebisu_polarssl_socket_t* sock = (ebisu_polarssl_socket_t*)arg;
    ebisu_socket_t* transport = sock->_transport;
    size_t sent = 0;
    uint32_t left_ms = 0;
    if (handshake_bounded(sock, &left_ms) && left_ms == 0) {
        sock->_timed_out = true;
        sock->_handshake_ran_out = true;
        return POLARSSL_ERR_NET_WANT_WRITE;
    }
    switch (transport->ops->send(transport, (const char*)buffer, length, &sent)) {
        case EBISU_SOCK_OK:
            if (sent > 0) {
                return (int)sent;
            }
            sock->_again = true;
            return POLARSSL_ERR_NET_WANT_WRITE;
        case EBISU_SOCK_AGAIN:
            sock->_again = true;
            return POLARSSL_ERR_NET_WANT_WRITE;
        default:
            take_transport_error(sock);
            if (sock->_transport_error == EBISU_SOCK_ERROR_TIMEOUT) {
                sock->_timed_out = true;
                return POLARSSL_ERR_NET_WANT_WRITE;
            }
            return POLARSSL_ERR_NET_SEND_FAILED;
    }
}

static int bio_recv(void* arg, unsigned char* buffer, size_t length)
{
    ebisu_polarssl_socket_t* sock = (ebisu_polarssl_socket_t*)arg;
    ebisu_socket_t* transport = sock->_transport;
    size_t received = 0;
    uint32_t left_ms = 0;
    /* In a handshake with a time to take, a wait for the server lasts what
       is left of it at the most: with the socket under it's recv_wait, where
       it has one, as its recv waits for its own timeout, or without end. */
    if (handshake_bounded(sock, &left_ms) &&
            (left_ms == 0 || (transport->ops->recv_wait != NULL &&
                              !transport->ops->recv_wait(transport, left_ms)))) {
        sock->_timed_out = true;
        sock->_handshake_ran_out = true;
        return POLARSSL_ERR_NET_WANT_READ;
    }
    switch (transport->ops->recv(transport, (char*)buffer, length, &received)) {
        case EBISU_SOCK_OK:
            /* 0, the end of the stream, as PolarSSL takes it. */
            return (int)received;
        case EBISU_SOCK_AGAIN:
            sock->_again = true;
            return POLARSSL_ERR_NET_WANT_READ;
        default: {
            bool timed_out;
            take_transport_error(sock);
            timed_out = transport->ops->recv_timed_out != NULL ?
                transport->ops->recv_timed_out(transport) :
                sock->_transport_error == EBISU_SOCK_ERROR_TIMEOUT;
            if (timed_out) {
                sock->_timed_out = true;
                return POLARSSL_ERR_NET_WANT_READ;
            }
            return POLARSSL_ERR_NET_RECV_FAILED;
        }
    }
}

/* The validity period of a certificate, at the clock's time. */
static ebisu_validity_t check_dates(ebisu_clock_t* clock, const x509_crt* crt)
{
    const x509_time* from = &crt->valid_from;
    const x509_time* to = &crt->valid_to;
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

/* Whether the SHA-256 of length bytes at p is pin. */
static bool hashes_to(const unsigned char* p, size_t length, const unsigned char* pin)
{
    unsigned char hash[32];
    unsigned char differ = 0;
    size_t i;
    if (p == NULL) {
        return false;
    }
    sha256(p, length, hash, 0);
    for (i = 0; i < EBISU_TLS_PIN_SIZE; ++i) {
        differ |= (unsigned char)(hash[i] ^ pin[i]);
    }
    return differ == 0;
}

/* Whether the SHA-256 of the certificate's SubjectPublicKeyInfo is pin:
   written again, as PolarSSL doesn't keep it, where it can write it. */
static bool public_key_hashes_to(const x509_crt* crt, const unsigned char* pin)
{
#if defined(POLARSSL_PK_WRITE_C)
    /* Room for an RSA key of 8192 bits'. Written from its end. */
    unsigned char der[1100];
    int length = pk_write_pubkey_der((pk_context*)&crt->pk, der, sizeof(der));
    return length > 0 && hashes_to(der + sizeof(der) - (size_t)length, (size_t)length, pin);
#else
    (void)crt;
    (void)pin;
    return false;
#endif
}

/* Whether the certificate is issued for the address, of length bytes: whether
   its subjectAltName has it as an iPAddress. PolarSSL keeps a certificate's
   names only, so this reads them from the extensions as they came, which
   PolarSSL has parsed: a SEQUENCE of extensions, each a SEQUENCE of an OID, a
   BOOLEAN that may be left out, and an OCTET STRING, which for a
   subjectAltName holds a SEQUENCE of names. */
static bool is_issued_for_address(const x509_crt* crt, const unsigned char* address,
        size_t length)
{
    static const unsigned char SUBJECT_ALT_NAME[] = { 0x55, 0x1d, 0x11 };
    const int sequence = ASN1_CONSTRUCTED | ASN1_SEQUENCE;
    unsigned char* p = crt->v3_ext.p;
    const unsigned char* end;
    size_t len = 0;
    if (p == NULL || crt->v3_ext.len == 0) {
        return false;
    }
    end = p + crt->v3_ext.len;
    if (asn1_get_tag(&p, end, &len, sequence) != 0) {
        return false;
    }
    end = p + len;
    while (p < end) {
        const unsigned char* extension_end;
        if (asn1_get_tag(&p, end, &len, sequence) != 0) {
            return false;
        }
        extension_end = p + len;
        if (asn1_get_tag(&p, extension_end, &len, ASN1_OID) != 0) {
            return false;
        }
        if (len != sizeof(SUBJECT_ALT_NAME) || memcmp(p, SUBJECT_ALT_NAME, len) != 0) {
            p = (unsigned char*)extension_end;
            continue;
        }
        p += len;
        if (p < extension_end && *p == ASN1_BOOLEAN) {
            if (asn1_get_tag(&p, extension_end, &len, ASN1_BOOLEAN) != 0) {
                return false;
            }
            p += len;
        }
        if (asn1_get_tag(&p, extension_end, &len, ASN1_OCTET_STRING) != 0 ||
                asn1_get_tag(&p, p + len, &len, sequence) != 0) {
            return false;
        }
        end = p + len;
        while (p < end) {
            /* [7], an iPAddress: its 4 or 16 bytes. */
            const unsigned char tag = *p++;
            if (asn1_get_len(&p, end, &len) != 0) {
                return false;
            }
            if (tag == (ASN1_CONTEXT_SPECIFIC | 7) && len == length &&
                    memcmp(p, address, length) == 0) {
                return true;
            }
            p += len;
        }
        /* A certificate has one subjectAltName at the most. */
        return false;
    }
    return false;
}

/* PolarSSL calls this for each certificate of the chain it built, from the
   CA's to the server's, with what it found wrong with it in flags. A pin is
   checked against the server's, depth 0's, the others taken; and no check
   takes them all. The server's name is checked in full only, as PolarSSL is
   given it only then. A host that is an address is checked here, PolarSSL
   having been given no name. With a clock, the validity periods are the
   clock's to check, in place of PolarSSL's check at the system's time, if it
   made one. */
static int verify(void* arg, x509_crt* crt, int depth, int* flags)
{
    ebisu_polarssl_socket_t* sock = (ebisu_polarssl_socket_t*)arg;
    const ebisu_polarssl_tls_t* tls = sock->_tls;
#if defined(EBISU_TLS_RESUME)
    if (depth == 0) {
        sock->_cert_checked = true;
    }
#endif
    switch (tls->_verify) {
        case EBISU_TLS_VERIFY_NONE:
            *flags = 0;
            return 0;
        case EBISU_TLS_VERIFY_PIN:
            /* The certificate, or its SubjectPublicKeyInfo. */
            *flags = depth != 0 || hashes_to(crt->raw.p, crt->raw.len, tls->_pin) ||
                public_key_hashes_to(crt, tls->_pin) ? 0 : BADCERT_OTHER;
            return 0;
        case EBISU_TLS_VERIFY_FULL:
            if (depth == 0 && sock->_address_length != 0 &&
                    !is_issued_for_address(crt, sock->_address, sock->_address_length)) {
                *flags |= BADCERT_CN_MISMATCH;
            }
            break;
        default:
            break;
    }
    if (sock->_clock == NULL) {
        return 0;
    }
    *flags &= ~(BADCERT_EXPIRED | BADCERT_FUTURE);
    switch (check_dates(sock->_clock, crt)) {
        case EBISU_VALIDITY_OK:
            break;
        case EBISU_VALIDITY_NOT_YET_VALID:
            *flags |= BADCERT_FUTURE;
            break;
        default:
            *flags |= BADCERT_EXPIRED;
            break;
    }
    return 0;
}

/* Whether a handshake failed as the server and the port have no TLS version or
   cipher suite in common: a version of the server's PolarSSL refused, or an
   alert of the server's that says so. PolarSSL gives any fatal alert as
   POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE, and leaves the alert where it read it,
   in the context's in_msg. */
static bool is_mismatch(const ssl_context* ssl, int ret)
{
    if (ret == POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION) {
        return true;
    }
    if (ret != POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE || ssl->in_msgtype != SSL_MSG_ALERT ||
            ssl->in_msg == NULL) {
        return false;
    }
    switch (ssl->in_msg[1]) {
        case SSL_ALERT_MSG_HANDSHAKE_FAILURE:
        case SSL_ALERT_MSG_PROTOCOL_VERSION:
        case SSL_ALERT_MSG_INSUFFICIENT_SECURITY:
            return true;
        default:
            return false;
    }
}

/* Why an operation failed, from PolarSSL's error, and what the socket under it
   said, for what failed there. */
static void set_tls_error(ebisu_polarssl_socket_t* sock, int ret)
{
    switch (ret) {
        case POLARSSL_ERR_NET_WANT_READ:
        case POLARSSL_ERR_NET_WANT_WRITE:
            set_error(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
            break;
        case POLARSSL_ERR_SSL_CONN_EOF:
        case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY:
            set_error(sock, EBISU_SOCK_ERROR_CLOSED, 0);
            break;
        case POLARSSL_ERR_NET_SEND_FAILED:
        case POLARSSL_ERR_NET_RECV_FAILED:
            set_error(sock, sock->_transport_error != EBISU_SOCK_ERROR_NONE ?
                    sock->_transport_error : EBISU_SOCK_ERROR_OTHER, sock->_transport_detail);
            break;
        default:
            set_error(sock, EBISU_SOCK_ERROR_TLS, ret);
            break;
    }
}

/* Whether PolarSSL's answer is that it wants to be called again, at once, with
   neither a timeout nor EBISU_SOCK_AGAIN of the socket under it. */
static bool call_again(const ebisu_polarssl_socket_t* sock, int ret)
{
    return (ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE) &&
        !sock->_timed_out && !sock->_again;
}

/* The verify flags of the handshake just made, or that failed, as 1.3.10 on's
   ssl_get_verify_result() gives them: 1.3.8's reads the session established,
   which a handshake that failed left none of, and crashes. */
static int verify_result(const ssl_context* ssl)
{
    if (ssl->session != NULL) {
        return ssl->session->verify_result;
    }
    if (ssl->session_negotiate != NULL) {
        return ssl->session_negotiate->verify_result;
    }
    return -1;
}

/* Done with the connection: with a close_notify, unless PolarSSL ended it. */
static void end(ebisu_polarssl_socket_t* sock, bool notify)
{
    if (sock->_open) {
        if (notify && !sock->_failed) {
            /* The server may have gone already. */
            (void)ssl_close_notify(&sock->_ssl);
        }
        ssl_free(&sock->_ssl);
        sock->_open = false;
    }
}

/* The connection's context, as the configuration has it, and the
   application's settings. PolarSSL's error, or 0. */
static int set_up(ebisu_polarssl_socket_t* sock, const char* host)
{
    ebisu_polarssl_tls_t* tls = sock->_tls;
    ssl_context* ssl = &sock->_ssl;
    int ret = ssl_init(ssl);
    /* Whatever ssl_init() took, even if it failed. */
    sock->_open = true;
    if (ret != 0) {
        return ret;
    }
    ssl_set_endpoint(ssl, SSL_IS_CLIENT);
    ssl_set_authmode(ssl, SSL_VERIFY_REQUIRED);
    ssl_set_min_version(ssl, SSL_MAJOR_VERSION_3, SSL_MINOR_VERSION_3);
    if (tls->_suites[0] != 0) {
        ssl_set_ciphersuites(ssl, tls->_suites);
    }
    /* The name to check the server's certificate for, in full only, and the
       one the server is to present a certificate for. An address is no name:
       the server is told none, as RFC 6066 has it, and verify() checks the
       certificate for the address, among its subjectAltNames of that kind,
       which PolarSSL doesn't read, and which no name of the same text is. An
       address is what a socket takes for one, its scope aside, which no
       certificate has. */
    ebisu_dns_addr_t address;
    sock->_address_length = 0;
    if (ebisu_dns_parse_address(NULL, host, strlen(host), 0, true, &address)) {
        sock->_address_length = address.family == EBISU_DNS_IPV4 ? 4 : 16;
        memcpy(sock->_address, address.bytes, sock->_address_length);
        host = NULL;
    }
    ssl_set_ca_chain(ssl, &tls->_ca, NULL, tls->_verify == EBISU_TLS_VERIFY_FULL ? host : NULL);
    ssl_set_verify(ssl, verify, sock);
#if EBISU_POLARSSL_SETS_RNG
    ssl_set_rng(ssl, drbg_random, tls);
#endif
    ssl_set_bio(ssl, bio_recv, sock, bio_send, sock);
#if defined(POLARSSL_SSL_SERVER_NAME_INDICATION)
    if (host != NULL) {
        ret = ssl_set_hostname(ssl, host);
        if (ret != 0) {
            return ret;
        }
    }
#endif
    if (tls->_setup != NULL) {
        tls->_setup(ssl, tls->_setup_userdata);
    }
    return 0;
}

#ifndef EBISU_NO_LOG
static bool is_among(const uint16_t* suites, size_t count, int id)
{
    for (size_t i = 0; i < count; ++i) {
        if (suites[i] == id) {
            return true;
        }
    }
    return false;
}

/* Says, once for a socket, that its connection's suite is none of the strict
   ones: the library has none, or the application set others. */
static void warn_of_suite(ebisu_polarssl_socket_t* sock, const char* host)
{
    const char* name = ssl_get_ciphersuite(&sock->_ssl);
    if (sock->_suite_warned || name == NULL ||
            is_among(STRICT_SUITES, COUNT_OF(STRICT_SUITES), ssl_get_ciphersuite_id(name))) {
        return;
    }
    sock->_suite_warned = true;
    log_to(sock->_logger, EBISU_LOG_WARNING,
            "connected to %s with %s, a cipher suite without forward secrecy or without "
            "authenticated encryption%s", host, name,
            is_among(STRICT_SUITES, COUNT_OF(STRICT_SUITES), sock->_tls->_suites[0]) ? "." :
            ": PolarSSL is built with none that has both.");
}
#else
#define warn_of_suite(sock, host) ((void)0)
#endif

#if defined(EBISU_TLS_RESUME)
/* How long a session is offered after the full handshake that checked the
   server's certificate. */
#define SESSION_LIFETIME_MS ((uint64_t)3600 * 1000)

/* FNV-1a, of 32 bits, of a host's name or a pin: a session is kept for the
   host, and the way its certificate was checked, without either. */
static uint32_t fnv1a(const unsigned char* p, size_t length)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; ++i) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t host_hash(const char* host)
{
    return fnv1a((const unsigned char*)host, strlen(host));
}

/* The configuration's pin, if it checks one, by its hash, or 0. */
static uint32_t pin_hash(const ebisu_polarssl_tls_t* tls)
{
    return tls->_verify == EBISU_TLS_VERIFY_PIN ? fnv1a(tls->_pin, EBISU_TLS_PIN_SIZE) : 0;
}

static void forget_session(ebisu_polarssl_socket_t* sock)
{
    if (sock->_has_session) {
        ssl_session_free(&sock->_session);
        ssl_session_init(&sock->_session);
        sock->_has_session = false;
    }
}

/* Offers the connection about to shake hands the session kept, if it is of
   the same host and port, and its certificate was checked less than
   SESSION_LIFETIME_MS ago, as the configuration checks them now: a session
   of a connection that checked none, resumed, would check none either. */
static void offer_session(ebisu_polarssl_socket_t* sock)
{
    sock->_cert_checked = false;
    if (!sock->_has_session) {
        return;
    }
    if (sock->_resume_system == NULL || sock->_session_host != sock->_host_hash ||
            sock->_session_port != sock->_port ||
            sock->_session_verify != sock->_tls->_verify ||
            sock->_session_pin != pin_hash(sock->_tls) ||
            ebisu_system_monotonic_ms(sock->_resume_system) - sock->_session_checked_ms >=
                SESSION_LIFETIME_MS) {
        forget_session(sock);
        return;
    }
    if (ssl_set_session(&sock->_ssl, &sock->_session) != 0) {
        forget_session(sock);
    }
}

/* Keeps the session of a connection whose handshake is done. */
static void keep_session(ebisu_polarssl_socket_t* sock)
{
    if (sock->_resume_system == NULL) {
        return;
    }
    /* A handshake that resumed a session checked no certificate: the one
       that made the session did, at the time kept with it. */
    bool full = sock->_cert_checked || sock->_tls->_verify == EBISU_TLS_VERIFY_NONE;
    if (!full && !sock->_has_session) {
        return;
    }
    uint64_t checked_ms = full ?
        ebisu_system_monotonic_ms(sock->_resume_system) : sock->_session_checked_ms;
    forget_session(sock);
    if (ssl_get_session(&sock->_ssl, &sock->_session) != 0) {
        ssl_session_free(&sock->_session);
        ssl_session_init(&sock->_session);
        return;
    }
    sock->_has_session = true;
    sock->_session_host = sock->_host_hash;
    sock->_session_port = sock->_port;
    sock->_session_checked_ms = checked_ms;
    sock->_session_verify = sock->_tls->_verify;
    sock->_session_pin = pin_hash(sock->_tls);
}
#endif

static ebisu_sock_code_t polarssl_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    ebisu_socket_t* transport = sock->_transport;
    int ret;
    set_error(sock, EBISU_SOCK_ERROR_NONE, 0);
    sock->_failed = false;
    sock->_recv_timed_out = false;
#if !defined(POLARSSL_HAVE_TIME)
    if (sock->_clock == NULL && (sock->_tls->_verify == EBISU_TLS_VERIFY_FULL ||
                sock->_tls->_verify == EBISU_TLS_VERIFY_CHAIN)) {
        /* Nothing to check the certificates' dates at. */
        log_to(sock->_logger, EBISU_LOG_ERROR,
                "no clock to check the certificate of %s at, and no time of the system's",
                host);
        set_error(sock, EBISU_SOCK_ERROR_TLS, 0);
        return EBISU_SOCK_FAIL;
    }
#endif
    /* Before connecting: none of it is sent. */
    ret = set_up(sock, host);
    if (ret != 0) {
        log_tls_error(sock->_logger, "failed to set up TLS for", host, &sock->_ssl, ret);
        set_error(sock, EBISU_SOCK_ERROR_TLS, ret);
        end(sock, false);
        return EBISU_SOCK_FAIL;
    }
#if !EBISU_POLARSSL_SETS_RNG
    if (sock->_ssl.f_rng == NULL) {
        /* PolarSSL's handshake would fail as it starts. */
        log_to(sock->_logger, EBISU_LOG_ERROR,
                "no random generator for TLS with %s: PolarSSL is built without "
                "POLARSSL_CTR_DRBG_C or POLARSSL_ENTROPY_C, and the application's setup set "
                "none with ssl_set_rng()", host);
        set_error(sock, EBISU_SOCK_ERROR_TLS, POLARSSL_ERR_SSL_NO_RNG);
        end(sock, false);
        return EBISU_SOCK_FAIL;
    }
#endif
    if (transport->ops->connect(transport, host, port) != EBISU_SOCK_OK) {
        take_transport_error(sock);
        set_error(sock, sock->_transport_error != EBISU_SOCK_ERROR_NONE ?
                sock->_transport_error : EBISU_SOCK_ERROR_CONNECT, sock->_transport_detail);
        end(sock, false);
        return EBISU_SOCK_FAIL;
    }
    if (sock->_clock != NULL) {
        int64_t now = 0;
        sock->_time_exact = ebisu_clock_get_time(sock->_clock, &now);
    }
#if defined(EBISU_TLS_RESUME)
    sock->_host_hash = host_hash(host);
    sock->_port = port;
    offer_session(sock);
#endif

    sock->_handshake_ran_out = false;
    if (sock->_handshake_ms != 0) {
        sock->_handshake_start_ms = ebisu_system_monotonic_ms(sock->_system);
        sock->_handshaking = true;
    }
    for (;;) {
        sock->_timed_out = false;
        sock->_again = false;
        ret = ssl_handshake(&sock->_ssl);
        /* Not timed out: called again, as for a socket that doesn't block,
           which connect waits for. */
        if ((ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE) &&
                !sock->_timed_out) {
            continue;
        }
        break;
    }
    sock->_handshaking = false;
    if (sock->_handshake_ran_out) {
        log_to(sock->_logger, EBISU_LOG_ERROR,
                "TLS handshake with %s not done in the %lu ms it has.", host,
                (unsigned long)sock->_handshake_ms);
    }
    if (ret == POLARSSL_ERR_X509_CERT_VERIFY_FAILED) {
        int flags = verify_result(&sock->_ssl);
        /* Not valid yet at the time the clock had, and nothing else wrong
           with it: more likely than a certificate from the future, the time
           fell behind. Checked at a lower bound, the next connection goes
           through, and its response gives the clock the time again. One that
           is refused for more than its dates says nothing of the time: anyone
           can make a certificate of any dates. */
        if (flags == BADCERT_FUTURE && sock->_clock != NULL && sock->_time_exact) {
            ebisu_clock_forget_time(sock->_clock);
        }
        if (sock->_tls->_verify == EBISU_TLS_VERIFY_PIN) {
            log_to(sock->_logger, EBISU_LOG_ERROR,
                    "server certificate for %s refused: neither it nor its public key has the pin",
                    host);
        } else {
            log_refused(sock->_logger, host, flags);
        }
        set_error(sock, EBISU_SOCK_ERROR_VERIFY, (long)flags);
        goto fail;
    } else if (ret != 0) {
        log_tls_error(sock->_logger, "TLS handshake failed with", host, &sock->_ssl, ret);
        if (is_mismatch(&sock->_ssl, ret)) {
            log_to(sock->_logger, EBISU_LOG_ERROR,
                    "%s may take only TLS older than 1.2, or none of the cipher suites "
                    "the configuration offers.", host);
            set_error(sock, EBISU_SOCK_ERROR_TLS_MISMATCH, ret);
        } else {
            set_tls_error(sock, ret);
        }
        goto fail;
    }
    if (sock->_tls->_verify == EBISU_TLS_VERIFY_NONE) {
        log_to(sock->_logger, EBISU_LOG_WARNING,
                "connected to %s without verifying its certificate.", host);
    }
    warn_of_suite(sock, host);
#if defined(EBISU_TLS_RESUME)
    keep_session(sock);
#endif
    return EBISU_SOCK_OK;

fail:
#if defined(EBISU_TLS_RESUME)
    /* The session offered may be why it failed. */
    forget_session(sock);
#endif
    /* No close_notify for a handshake that didn't end. */
    end(sock, false);
    (void)transport->ops->close(transport);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t polarssl_send(
        ebisu_socket_t* self,
        const char* buffer,
        size_t length,
        size_t* out_sent_length)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    int ret;
    *out_sent_length = 0;
    do {
        sock->_timed_out = false;
        sock->_again = false;
        ret = ssl_write(&sock->_ssl, (const unsigned char*)buffer, length);
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
        log_tls_error(sock->_logger, "failed to send to", "the server", &sock->_ssl, ret);
    }
    set_tls_error(sock, ret);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t polarssl_recv(
        ebisu_socket_t* self,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    int ret;
    *out_actual_length = 0;
    sock->_recv_timed_out = false;
    do {
        sock->_timed_out = false;
        sock->_again = false;
        ret = ssl_read(&sock->_ssl, (unsigned char*)buffer, length_to_read);
    } while (call_again(sock, ret));
    if (ret > 0) {
        *out_actual_length = (size_t)ret;
        return EBISU_SOCK_OK;
    }
    if (ret == 0 || ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY || ret == POLARSSL_ERR_SSL_CONN_EOF) {
        /* The end of the stream. */
        return EBISU_SOCK_OK;
    }
    if (ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE) {
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
    log_tls_error(sock->_logger, "failed to receive from", "the server", &sock->_ssl, ret);
    set_tls_error(sock, ret);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t polarssl_close(ebisu_socket_t* self)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    if (!sock->_open) {
        return EBISU_SOCK_OK;
    }
    end(sock, true);
    return sock->_transport->ops->close(sock->_transport);
}

static bool polarssl_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    return sock->_recv_timed_out;
}

static ebisu_sock_error_t polarssl_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    *out_detail = sock->_error_detail;
    return sock->_error;
}

/* Whether PolarSSL holds what a read can go on with, beyond the application
   data ssl_get_bytes_avail() counts: more handshake messages in the record it
   read, as it checks for them. */
static bool has_pending(const ssl_context* ssl)
{
    return ssl->in_hslen > 0 && ssl->in_hslen < ssl->in_msglen;
}

/* What PolarSSL has decrypted and recv hasn't returned, or has of a record, is
   for recv to return without waiting. */
static bool polarssl_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_polarssl_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_polarssl_socket_t, base);
    if (ssl_get_bytes_avail(&sock->_ssl) > 0 || has_pending(&sock->_ssl)) {
        return true;
    }
    return sock->_transport->ops->recv_wait(sock->_transport, timeout_ms);
}

/* With recv_wait when the socket under it has one, and without when it hasn't,
   for the MQTT task to wait in recv instead. */
static const ebisu_socket_ops_t ops_waiting = {
    polarssl_connect,
    polarssl_send,
    polarssl_recv,
    polarssl_close,
    polarssl_recv_timed_out,
    polarssl_last_error,
    polarssl_recv_wait
};

static const ebisu_socket_ops_t ops = {
    polarssl_connect,
    polarssl_send,
    polarssl_recv,
    polarssl_close,
    polarssl_recv_timed_out,
    polarssl_last_error,
    NULL
};

void ebisu_polarssl_socket_init(
        ebisu_polarssl_socket_t* sock,
        ebisu_socket_t* transport,
        ebisu_polarssl_tls_t* tls)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = transport->ops->recv_wait != NULL ? &ops_waiting : &ops;
    sock->_transport = transport;
    sock->_tls = tls;
#if defined(EBISU_TLS_RESUME)
    ssl_session_init(&sock->_session);
#endif
}

void ebisu_polarssl_socket_set_handshake_timeout(ebisu_polarssl_socket_t* sock,
        ebisu_system_t* system, unsigned int seconds)
{
    if (system == NULL || system->ops == NULL || system->ops->monotonic_ms == NULL) {
        seconds = 0;
    }
    sock->_system = system;
    sock->_handshake_ms = seconds > UINT32_MAX / 1000 ? UINT32_MAX : (uint32_t)seconds * 1000;
}

void ebisu_polarssl_socket_set_clock(ebisu_polarssl_socket_t* sock, ebisu_clock_t* clock)
{
    sock->_clock = clock;
}

void ebisu_polarssl_socket_set_logger(ebisu_polarssl_socket_t* sock, ebisu_logger_t* logger)
{
    sock->_logger = logger;
}

#if defined(EBISU_TLS_RESUME)
void ebisu_polarssl_socket_set_session_resume(ebisu_polarssl_socket_t* sock,
        ebisu_system_t* system)
{
    sock->_resume_system = system;
    if (system == NULL) {
        forget_session(sock);
    }
}
#endif

const char* ebisu_polarssl_socket_get_protocol(const ebisu_polarssl_socket_t* sock)
{
    return sock->_open ? ssl_get_version(&sock->_ssl) : NULL;
}

const char* ebisu_polarssl_socket_get_cipher(const ebisu_polarssl_socket_t* sock)
{
    return sock->_open ? ssl_get_ciphersuite(&sock->_ssl) : NULL;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
