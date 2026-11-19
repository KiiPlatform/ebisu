/**
 * @file ebisu_polarssl.h
 * @brief TLS with PolarSSL 1.3, over another socket of the system's:
 * experimental.
 *
 * As the mbedTLS port's socket, ebisu_mbedtls.h, for PolarSSL 1.3, mbedTLS's
 * name until 1.3.10, whose API mbedTLS 1.3 kept until its last release,
 * 1.3.22: for vendors' SDKs that ship it, some with its cryptography in their
 * chips' ROM. Experimental: its API may change. PolarSSL 1.3 is past its end
 * of life: its last release, 1.3.22, came in February 2018, and 1.3.8, of July
 * 2014, lacks the security fixes of each of the 14 since. Use the mbedTLS port
 * where mbedTLS 2.4 or later can be had.
 */
#ifndef _EBISU_POLARSSL
#define _EBISU_POLARSSL

#include <ebisu_clock.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>

#include <polarssl/ssl.h>
#include <polarssl/x509_crt.h>

/**
 * \brief Whether ebisu_polarssl_tls_init() sets the connections' random
 * generator: 1, but for PolarSSL built without POLARSSL_CTR_DRBG_C or
 * POLARSSL_ENTROPY_C, where it is 0, and the application sets one.
 */
#if defined(POLARSSL_CTR_DRBG_C) && defined(POLARSSL_ENTROPY_C)
#define EBISU_POLARSSL_SETS_RNG 1
#else
#define EBISU_POLARSSL_SETS_RNG 0
#endif
#if EBISU_POLARSSL_SETS_RNG
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>
#if defined(POLARSSL_THREADING_C)
#include <polarssl/threading.h>
#endif
#endif

#include <stdbool.h>
#include <stddef.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Settings of the application's for each connection.
 *
 * Called with each connection's context, after the port's own settings and
 * before the handshake, for those of the application's: cipher suites, with
 * ssl_set_ciphersuites(), or, for PolarSSL built with SSL_MAX_CONTENT_LEN
 * smaller than the 16 KB a server may send, the maximum fragment length to
 * ask the server for, with ssl_set_max_frag_len(), which a server that doesn't
 * take the extension ignores; and the random generator, with ssl_set_rng(),
 * where EBISU_POLARSSL_SETS_RNG is 0. What the port set, the verification, the
 * CA certificates, the host name, the lowest version and the generator where
 * EBISU_POLARSSL_SETS_RNG is 1, are the port's to keep.
 *
 * \param [in,out] ssl The connection's context.
 * \param [in] userdata What ebisu_polarssl_tls_set_setup() was given.
 */
typedef void (*EBISU_POLARSSL_SETUP)(ssl_context* ssl, void* userdata);

/**
 * \brief The TLS configuration connections share.
 *
 * TLS 1.2, with PolarSSL's default cipher suites, which a server picks one of.
 * The server's certificate has to chain to one of the configuration's CA
 * certificates, and be issued for the host connected to, unless
 * ebisu_polarssl_tls_set_verify() says otherwise. PolarSSL 1.3 has no
 * configuration shared by connections, as mbedTLS 2 on has: the port keeps
 * this one, and sets each connection's context from it as it connects.
 *
 * Random numbers come from a CTR_DRBG of the configuration's own, which
 * ebisu_polarssl_tls_init() seeds from PolarSSL's entropy sources, the
 * hardware's with POLARSSL_ENTROPY_HARDWARE_ALT. PolarSSL built without
 * POLARSSL_CTR_DRBG_C or POLARSSL_ENTROPY_C, as some of vendors' SDKs
 * configure it, has neither, and EBISU_POLARSSL_SETS_RNG is 0: the
 * application sets the generator, with ssl_set_rng() in its setup, which
 * ebisu_polarssl_tls_set_setup() sets, and a connection fails without one.
 * TLS's keys are made from these numbers, whichever gives them: the entropy
 * sources have to give true randomness, and the application's generator has
 * to be cryptographically secure, a hardware random number generator's, or a
 * DRBG seeded from one. Connections share the generator: on more than one task
 * at once, PolarSSL has to be built with POLARSSL_THREADING_C, with
 * POLARSSL_THREADING_PTHREAD on POSIX, or POLARSSL_THREADING_ALT and functions
 * of the system's, which the port locks its CTR_DRBG with, as PolarSSL's own
 * doesn't; or the application's generator has to take calls from more than
 * one task at once.
 *
 * Initialize with ebisu_polarssl_tls_init(), before the connections, and free
 * with ebisu_polarssl_tls_free() once none is open.
 */
typedef struct ebisu_polarssl_tls_t {
    x509_crt _ca; /**< \private */
    ebisu_tls_verify_t _verify; /**< \private */
    unsigned char _pin[EBISU_TLS_PIN_SIZE]; /**< \private */
    EBISU_POLARSSL_SETUP _setup; /**< \private */
    void* _setup_userdata; /**< \private */
#if EBISU_POLARSSL_SETS_RNG
    entropy_context _entropy; /**< \private */
    ctr_drbg_context _drbg; /**< \private */
#if defined(POLARSSL_THREADING_C)
    threading_mutex_t _drbg_lock; /**< \private */
#endif
#endif
} ebisu_polarssl_tls_t;

/**
 * \brief Sets up a TLS configuration, with the CA certificates the server's
 * has to chain to.
 *
 * Seeds the configuration's CTR_DRBG, which fails without an entropy source
 * PolarSSL takes for a strong one, but where EBISU_POLARSSL_SETS_RNG is 0:
 * there it sets no random generator, which the application sets.
 *
 * \param [out] tls The configuration.
 * \param [in] ca The CA certificates: PEM, one or more, with the terminating
 * NUL, or one in DER. NULL, with a ca_size of 0, for none, for a
 * configuration that pins the server's certificate or checks none, with
 * ebisu_polarssl_tls_set_verify(): checked in full, no server's is taken.
 * \param [in] ca_size The size of ca, its NUL included for PEM.
 * \return 0, or PolarSSL's error, in which case the configuration holds
 * nothing to free.
 */
int ebisu_polarssl_tls_init(ebisu_polarssl_tls_t* tls, const unsigned char* ca, size_t ca_size);

/**
 * \brief Sets the application's settings for each connection.
 *
 * Set before the first connection. See EBISU_POLARSSL_SETUP.
 *
 * \param [in,out] tls The configuration.
 * \param [in] setup The settings, or NULL for none, as the configuration
 * starts.
 * \param [in] userdata What setup is given.
 */
void ebisu_polarssl_tls_set_setup(ebisu_polarssl_tls_t* tls, EBISU_POLARSSL_SETUP setup,
        void* userdata);

/**
 * \brief Sets how the configuration's connections check the server's
 * certificate.
 *
 * Every connection is checked in full unless this says otherwise: see
 * ebisu_tls_verify_t for what each mode checks, and what it doesn't. With
 * EBISU_TLS_VERIFY_CHAIN, the chain and the dates are checked as in full, at
 * the clock's time; with EBISU_TLS_VERIFY_PIN or EBISU_TLS_VERIFY_NONE, they
 * aren't, the configuration's CA certificates aren't used, and a socket
 * without a clock connects without the system's time too. A pin is the
 * SHA-256 of the server's certificate in DER, or of its public key's
 * SubjectPublicKeyInfo in DER, which PolarSSL doesn't keep, and is written
 * again with POLARSSL_PK_WRITE_C, or else not taken. Set it before the first
 * connection.
 *
 * \param [in,out] tls The configuration.
 * \param [in] verify How servers' certificates are checked.
 * \param [in] pin With EBISU_TLS_VERIFY_PIN, the EBISU_TLS_PIN_SIZE bytes of
 * the pin; otherwise ignored, and may be NULL.
 * \return true, or false, leaving the configuration as it was, for
 * EBISU_TLS_VERIFY_PIN without a pin, or a mode that isn't one.
 */
bool ebisu_polarssl_tls_set_verify(ebisu_polarssl_tls_t* tls, ebisu_tls_verify_t verify,
        const unsigned char* pin);

/**
 * \brief Frees a TLS configuration, which no connection may use any more.
 *
 * \param [in,out] tls The configuration.
 */
void ebisu_polarssl_tls_free(ebisu_polarssl_tls_t* tls);

/**
 * \brief A TLS connection with PolarSSL, over another socket of the system's.
 *
 * The socket under it connects to the host, sends and receives the TLS
 * records, and closes: a socket of plain TCP, such as ebisu_posix_tcp_socket_t,
 * whose timeouts are the connection's. A receive that times out is
 * EBISU_SOCK_FAIL, which recv_timed_out tells from a lost connection, and
 * leaves the connection as it was. A send that times out fails too.
 *
 * The server's certificate is checked as the configuration says, and the
 * validity period of each of its chain's at the time of the clock set by
 * ebisu_polarssl_socket_set_clock(), in place of the check PolarSSL makes
 * itself with POLARSSL_HAVE_TIME. Without a clock, PolarSSL checks them at the
 * system's time, with POLARSSL_HAVE_TIME; without that either, the socket
 * doesn't connect, rather than not check them.
 *
 * A close_notify of the server's, or the end of the stream under it, is the end
 * of the stream. recv_wait, when the socket under it has one, counts what
 * PolarSSL has received and not yet returned.
 *
 * Why an operation failed, its last_error says: what the socket under it said,
 * for what failed there, as a host that didn't resolve or an address that
 * refused, a certificate refused, with PolarSSL's verify flags, BADCERT_*, a
 * server with no TLS version or cipher suite in common with the configuration,
 * EBISU_SOCK_ERROR_TLS_MISMATCH, with PolarSSL's error, another TLS failure,
 * with PolarSSL's error, a timeout, or a peer that closed. The TLS failures
 * also go to the logger set by ebisu_polarssl_socket_set_logger(), if any, with
 * a line for the mismatch that says so; the socket under it logs its own, to
 * the logger it is given.
 *
 * Each connection takes PolarSSL's buffers, of SSL_BUFFER_LEN each way, 16 KB
 * and some by default, from the heap, with polarssl_malloc(). One serves one
 * connection at a time, from one task at a time, and is used again for the
 * next once closed. Initialize with ebisu_polarssl_socket_init().
 */
typedef struct ebisu_polarssl_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    ebisu_socket_t* _transport; /**< \private */
    ebisu_polarssl_tls_t* _tls; /**< \private */
    ebisu_clock_t* _clock; /**< \private */
    ebisu_logger_t* _logger; /**< \private */
    ssl_context _ssl; /**< \private */
    /** \private Whether _ssl is set up, from connect to close. */
    bool _open;
    /** \private Whether PolarSSL ended the connection with an error, after
     * which it sends no close_notify. */
    bool _failed;
    /** \private Whether the clock had the time, rather than a lower bound
     * only, when the connection was made. */
    bool _time_exact;
    /** \private Whether the socket under it timed out in the last operation,
     * which PolarSSL was told is wanting to read or write. */
    bool _timed_out;
    /** \private Whether it said to try again, EBISU_SOCK_AGAIN, as a socket
     * that doesn't block does, which PolarSSL was told so too. */
    bool _again;
    /** \private Whether the last receive failed for its timeout. */
    bool _recv_timed_out;
    /** \private What the socket under it said of its last failure. */
    ebisu_sock_error_t _transport_error;
    long _transport_detail; /**< \private */
    /** \private Why the last operation that returned EBISU_SOCK_FAIL failed,
     * for last_error, and more of it. */
    ebisu_sock_error_t _error;
    long _error_detail; /**< \private */
} ebisu_polarssl_socket_t;

/**
 * \brief Clears the socket, over the socket under it and with the
 * configuration, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] transport The socket under it, which has to outlive it, and
 * isn't used by anything else meanwhile.
 * \param [in] tls The configuration, set up with ebisu_polarssl_tls_init(),
 * which has to outlive the socket.
 */
void ebisu_polarssl_socket_init(
        ebisu_polarssl_socket_t* sock,
        ebisu_socket_t* transport,
        ebisu_polarssl_tls_t* tls);

/**
 * \brief Sets the clock the server's certificate is checked at the time of.
 *
 * While the clock has only a lower bound of the time, a certificate that
 * expired before it is refused, and one not valid yet is not, as it may have
 * been issued since. A certificate not valid yet at a time the clock had is
 * refused, and the clock's time forgotten, so that the next connection is
 * checked at a lower bound and its response gives the clock the time again:
 * more likely than a certificate from the future, the time fell behind. See
 * ebisu_clock_forget_time().
 *
 * \param [out] sock The socket.
 * \param [in] clock The clock, which has to outlive the socket, or NULL, as the
 * socket starts, for the system's time.
 */
void ebisu_polarssl_socket_set_clock(ebisu_polarssl_socket_t* sock, ebisu_clock_t* clock);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Why TLS failed, at EBISU_LOG_ERROR: the handshake, a certificate refused,
 * with why, a send or a receive. A receive that times out isn't one, and the
 * socket under it logs its own failures.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_polarssl_socket_set_logger(ebisu_polarssl_socket_t* sock, ebisu_logger_t* logger);

/**
 * \brief The TLS version the connection open negotiated.
 *
 * As PolarSSL names it, with ssl_get_version(): "TLSv1.2".
 *
 * \param [in] sock The socket.
 * \return The version, which lasts as long as the connection does, or NULL
 * while there is none: before a connect, after one that failed, and once
 * closed.
 */
const char* ebisu_polarssl_socket_get_protocol(const ebisu_polarssl_socket_t* sock);

/**
 * \brief The cipher suite the connection open negotiated.
 *
 * As PolarSSL names it, with ssl_get_ciphersuite(): the standard name with
 * dashes, such as "TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256".
 *
 * \param [in] sock The socket.
 * \return The suite, which lasts as long as the connection does, or NULL
 * while there is none.
 */
const char* ebisu_polarssl_socket_get_cipher(const ebisu_polarssl_socket_t* sock);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_POLARSSL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
