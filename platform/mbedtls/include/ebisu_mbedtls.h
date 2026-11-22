/**
 * @file ebisu_mbedtls.h
 * @brief TLS with mbedTLS, over another socket of the system's.
 */
#ifndef EBISU_MBEDTLS_H
#define EBISU_MBEDTLS_H

#include <ebisu_clock.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>

#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
/* The version, which mbedTLS 2 has in a header of its own. */
#if !defined(MBEDTLS_VERSION_NUMBER)
#include <mbedtls/version.h>
#endif

/**
 * \brief Whether ebisu_mbedtls_tls_init() sets the configuration's random
 * generator: 1, but for mbedTLS 2 built without MBEDTLS_CTR_DRBG_C or
 * MBEDTLS_ENTROPY_C, where it is 0, and the application sets one.
 */
#if MBEDTLS_VERSION_MAJOR >= 3 || (defined(MBEDTLS_CTR_DRBG_C) && defined(MBEDTLS_ENTROPY_C))
#define EBISU_MBEDTLS_SETS_RNG 1
#else
#define EBISU_MBEDTLS_SETS_RNG 0
#endif
#if MBEDTLS_VERSION_MAJOR < 3 && EBISU_MBEDTLS_SETS_RNG
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
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

/** \brief The most cipher suites a configuration of the port's offers. */
#define EBISU_MBEDTLS_SUITES_MAX 18

/**
 * \brief The TLS configuration connections share.
 *
 * TLS 1.2 or later, 1.3 where mbedTLS is built with it, with mbedTLS's default
 * cipher suites. The server's certificate has to chain to one of the
 * configuration's CA certificates, and be issued for the host connected to,
 * unless ebisu_mbedtls_tls_set_verify() says otherwise.
 *
 * Random numbers come from PSA's generator, which ebisu_mbedtls_tls_init()
 * starts with psa_crypto_init(), from mbedTLS's entropy sources: on a device,
 * the hardware's, as mbedTLS is configured to take them, with
 * MBEDTLS_ENTROPY_HARDWARE_ALT in mbedTLS 3.6, or
 * MBEDTLS_PSA_DRIVER_GET_ENTROPY in 4.x. With mbedTLS 2, they come from a
 * CTR_DRBG of the configuration's own, which ebisu_mbedtls_tls_init() seeds
 * from mbedTLS's entropy sources, the hardware's with
 * MBEDTLS_ENTROPY_HARDWARE_ALT. mbedTLS 2 built without MBEDTLS_CTR_DRBG_C or
 * MBEDTLS_ENTROPY_C, as some of vendors' SDKs configure it, has neither, and
 * EBISU_MBEDTLS_SETS_RNG is 0: the application sets the generator, with
 * mbedtls_ssl_conf_rng() on ebisu_mbedtls_tls_config(), before the first
 * connection, which fails without one. TLS's keys are made from these
 * numbers, whichever gives them: the entropy sources have to give true
 * randomness, and the application's generator has to be cryptographically
 * secure, a hardware random number generator's, or a DRBG seeded from one.
 * Connections share the generator: on more than one task at once, mbedTLS has
 * to be built with MBEDTLS_THREADING_C, with MBEDTLS_THREADING_PTHREAD on
 * POSIX, or MBEDTLS_THREADING_ALT and functions of the system's, or the
 * application's generator has to take calls from more than one task at
 * once.
 *
 * It offers the cipher suites with forward secrecy and authenticated
 * encryption that mbedTLS is built with: TLS 1.3's, and TLS 1.2's with an
 * ECDHE or DHE key exchange and AES-GCM, ChaCha20-Poly1305 or AES-CCM. An
 * mbedTLS built with none of them, as some of vendors' SDKs build it, offers
 * those it has of AES in CBC mode or with an RSA key exchange instead, and the
 * socket's logger warns of the suite a connection then has, once a socket.
 * Nothing else is offered in either case: no RC4, 3DES, Camellia or ARIA, no
 * NULL cipher, and no pre-shared key. Other suites are the application's to
 * set, with mbedtls_ssl_conf_ciphersuites() on ebisu_mbedtls_tls_config(),
 * and a connection with one that lacks either gets the same warning.
 *
 * mbedTLS 3.6 and 4.x, and mbedTLS 2 from 2.4, as vendors' SDKs ship it,
 * which has TLS 1.2 at the most. mbedTLS 2 is past its end of life: its last
 * release, 2.28.10, came in March 2025, and an earlier 2 lacks the fixes
 * since it, 2.4.0, of 2016, those of most of a decade. Initialize with
 * ebisu_mbedtls_tls_init(), before the connections, and free with
 * ebisu_mbedtls_tls_free() once none is open.
 */
typedef struct ebisu_mbedtls_tls_t {
    mbedtls_ssl_config _config; /**< \private */
    mbedtls_x509_crt _ca; /**< \private */
    ebisu_tls_verify_t _verify; /**< \private */
    unsigned char _pin[EBISU_TLS_PIN_SIZE]; /**< \private */
    /** \private The cipher suites it offers, ended by a 0. */
    int _suites[EBISU_MBEDTLS_SUITES_MAX + 1];
#if MBEDTLS_VERSION_MAJOR < 3 && EBISU_MBEDTLS_SETS_RNG
    mbedtls_entropy_context _entropy; /**< \private */
    mbedtls_ctr_drbg_context _drbg; /**< \private */
#endif
} ebisu_mbedtls_tls_t;

/**
 * \brief Sets up a TLS configuration, with the CA certificates the server's
 * has to chain to.
 *
 * With mbedTLS 3.6 or 4.x, calls psa_crypto_init(), which may have been
 * called already, and doesn't call mbedtls_psa_crypto_free(), which the
 * application may, once it no longer uses PSA. With mbedTLS 2, seeds the
 * configuration's CTR_DRBG, which fails without an entropy source mbedTLS
 * takes for a strong one, but where EBISU_MBEDTLS_SETS_RNG is 0: there it sets
 * no random generator, which the application sets.
 *
 * \param [out] tls The configuration.
 * \param [in] ca The CA certificates: PEM, one or more, with the terminating
 * NUL, or one in DER. NULL, with a ca_size of 0, for none, for a
 * configuration that pins the server's certificate or checks none, with
 * ebisu_mbedtls_tls_set_verify(): checked in full, no server's is taken.
 * \param [in] ca_size The size of ca, its NUL included for PEM.
 * \param [out] out_error MbedTLS's or PSA's error when it fails, and 0 when it doesn't:
 * which certificate it couldn't read, or that it has no entropy source, for a
 * log. NULL for none.
 * \return Whether it did. If not, the configuration holds nothing to free.
 */
bool ebisu_mbedtls_tls_init(ebisu_mbedtls_tls_t* tls, const unsigned char* ca, size_t ca_size,
        int* out_error);

/**
 * \brief mbedTLS's configuration, for settings of the application's.
 *
 * Such as the cipher suites, mbedtls_ssl_conf_ciphersuites(), or the lowest
 * version, mbedtls_ssl_conf_min_tls_version(), mbedtls_ssl_conf_min_version()
 * in mbedTLS 2. Or, for an mbedTLS built with a record smaller than the 16 KB a
 * server may send, MBEDTLS_SSL_IN_CONTENT_LEN or, before 2.12,
 * MBEDTLS_SSL_MAX_CONTENT_LEN, the maximum fragment length to ask the server
 * for, mbedtls_ssl_conf_max_frag_len(), which a server that doesn't take the
 * extension ignores. A record longer than mbedTLS takes fails the connection,
 * and the socket's logger says so, with the record's length. Changed after
 * ebisu_mbedtls_tls_init(), before the first connection. What
 * ebisu_mbedtls_tls_init() set, the verification, the CA certificates and the
 * random generator, are the socket's to keep, but for the random generator
 * where EBISU_MBEDTLS_SETS_RNG is 0, which the application sets here, with
 * mbedtls_ssl_conf_rng().
 *
 * \param [in] tls The configuration.
 * \return mbedTLS's configuration.
 */
mbedtls_ssl_config* ebisu_mbedtls_tls_config(ebisu_mbedtls_tls_t* tls);

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
 * SubjectPublicKeyInfo in DER, which mbedTLS before 2.17 doesn't keep, and is
 * written again with MBEDTLS_PK_WRITE_C, or else not taken. Set it before the
 * first connection.
 *
 * \param [in,out] tls The configuration.
 * \param [in] verify How servers' certificates are checked.
 * \param [in] pin With EBISU_TLS_VERIFY_PIN, the EBISU_TLS_PIN_SIZE bytes of
 * the pin; otherwise ignored, and may be NULL.
 * \return true, or false, leaving the configuration as it was, for
 * EBISU_TLS_VERIFY_PIN without a pin, or a mode that isn't one.
 */
bool ebisu_mbedtls_tls_set_verify(ebisu_mbedtls_tls_t* tls, ebisu_tls_verify_t verify,
        const unsigned char* pin);

/**
 * \brief Frees a TLS configuration, which no connection may use any more.
 *
 * \param [in,out] tls The configuration.
 */
void ebisu_mbedtls_tls_free(ebisu_mbedtls_tls_t* tls);

/**
 * \brief A TLS connection with mbedTLS, over another socket of the system's.
 *
 * The socket under it connects to the host, sends and receives the TLS
 * records, and closes: a socket of plain TCP, such as ebisu_posix_tcp_socket_t,
 * whose timeouts are the connection's. A receive that times out is
 * EBISU_SOCK_FAIL, which recv_timed_out tells from a lost connection, and
 * leaves the connection as it was. A send that times out fails too.
 *
 * The server's certificate is checked as the configuration says, and the
 * validity period of each of its chain's at the time of the clock set by
 * ebisu_mbedtls_socket_set_clock(), in place of the check mbedTLS makes itself
 * with MBEDTLS_HAVE_TIME_DATE. Without a clock, mbedTLS checks them at the
 * system's time, with MBEDTLS_HAVE_TIME_DATE; without that either, the socket
 * doesn't connect, rather than not check them.
 *
 * A host given as an IPv4 or IPv6 address, in any form
 * ebisu_dns_parse_address() takes, is checked against the addresses the
 * certificate is issued for, by the socket itself, whatever the version of
 * mbedTLS, and not against its names. The server is sent no server name then.
 *
 * A close_notify of the server's, or the end of the stream under it, is the end
 * of the stream. A TLS 1.3 server's session tickets are taken and dropped.
 * recv_wait, when the socket under it has one, counts what mbedTLS has
 * received and not yet returned.
 *
 * With mbedTLS before 2.18, which sets a verify callback for a configuration
 * only, the socket has a copy of the configuration's of its own, taken at each
 * connect, with its own callback.
 *
 * Why an operation failed, its last_error says: what the socket under it said,
 * for what failed there, as a host that didn't resolve or an address that
 * refused, a certificate refused, with mbedTLS's verify flags,
 * MBEDTLS_X509_BADCERT_*, a server with no TLS version or cipher suite in
 * common with the configuration, EBISU_SOCK_ERROR_TLS_MISMATCH, with
 * mbedTLS's error, another TLS failure, with mbedTLS's error, a timeout, or a
 * peer that closed. The TLS failures also go to the logger set by
 * ebisu_mbedtls_socket_set_logger(), if any, with a line for the mismatch that
 * says so; the socket under it logs its own, to the logger it is given.
 *
 * One serves one connection at a time, from one task at a time, and is used
 * again for the next once closed. Initialize with ebisu_mbedtls_socket_init().
 */
typedef struct ebisu_mbedtls_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    ebisu_socket_t* _transport; /**< \private */
    ebisu_mbedtls_tls_t* _tls; /**< \private */
    ebisu_clock_t* _clock; /**< \private */
    ebisu_logger_t* _logger; /**< \private */
    mbedtls_ssl_context _ssl; /**< \private */
    /** \private Whether _ssl is set up, from connect to close. */
    bool _open;
    /** \private Whether mbedTLS ended the connection with an error, after
     * which it sends no close_notify. */
    bool _failed;
    /** \private Whether the clock had the time, rather than a lower bound
     * only, when the connection was made. */
    bool _time_exact;
    /** \private Whether the socket under it timed out in the last operation,
     * which mbedTLS was told is wanting to read or write. */
    bool _timed_out;
    /** \private Whether it said to try again, EBISU_SOCK_AGAIN, as a socket
     * that doesn't block does, which mbedTLS was told so too. */
    bool _again;
    /** \private Whether the last receive failed for its timeout. */
    bool _recv_timed_out;
    /** \private What the socket under it said of its last failure. */
    ebisu_sock_error_t _transport_error;
    long _transport_detail; /**< \private */
    /** \private The system whose monotonic_ms times the handshake, and how
     * long it may take, in milliseconds, or 0 for no bound of its own. */
    ebisu_system_t* _system;
    uint32_t _handshake_ms; /**< \private */
    /** \private When the handshake under way started, whether one is, and
     * whether the last ran out of its time. */
    uint64_t _handshake_start_ms;
    bool _handshaking; /**< \private */
    bool _handshake_ran_out; /**< \private */
    /** \private Whether it has said that a connection's cipher suite is
     * none of those the port would have. */
    bool _suite_warned;
    /** \private The address the connection's host is, of 4 or 16 bytes, for
     * the certificate to be checked for; 0 bytes for a name. */
    unsigned char _address_length;
    unsigned char _address[16]; /**< \private */
    /** \private Why the last operation that returned EBISU_SOCK_FAIL failed,
     * for last_error, and more of it. */
    ebisu_sock_error_t _error;
    long _error_detail; /**< \private */
#if MBEDTLS_VERSION_NUMBER < 0x02120000
    /** \private The configuration's, copied at connect, with a verify
     * callback of the socket's, which mbedTLS before 2.18 sets for a
     * configuration only. Not freed: what it points to is the
     * configuration's. */
    mbedtls_ssl_config _config;
#endif
} ebisu_mbedtls_socket_t;

/**
 * \brief Clears the socket, over the socket under it and with the
 * configuration, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] transport The socket under it, which has to outlive it, and
 * isn't used by anything else meanwhile.
 * \param [in] tls The configuration, set up with ebisu_mbedtls_tls_init(),
 * which has to outlive the socket.
 */
void ebisu_mbedtls_socket_init(
        ebisu_mbedtls_socket_t* sock,
        ebisu_socket_t* transport,
        ebisu_mbedtls_tls_t* tls);

/**
 * \brief Sets how long the TLS handshake may take.
 *
 * The socket under it bounds each wait for the server with its receive
 * timeout, and not how many there are in a handshake: a server that takes
 * the connection and says nothing to a socket with no receive timeout, or
 * sends a little at a time, keeps connect from returning. With this set, the
 * handshake fails as a timeout, EBISU_SOCK_ERROR_TIMEOUT, once it has taken
 * this long, by the system's monotonic_ms.
 *
 * It waits for the server with the recv_wait of the socket under it, for
 * what is left of the time, where that socket has one, as platform's have:
 * its receive timeout doesn't bound a wait of the handshake's then. Without
 * recv_wait, the time is looked at before each receive and send, so a wait
 * lasts as long as the socket under it lets it, and the handshake ends once
 * that wait does.
 *
 * Unset, as the socket starts, the handshake is bounded by the socket under
 * it alone. Set it before connecting.
 *
 * \param [out] sock The socket.
 * \param [in] system The system, for its monotonic_ms, which has to outlive
 * the socket. With NULL, or one without monotonic_ms, there is no bound.
 * \param [in] seconds How long, in seconds, or 0 for no bound.
 */
void ebisu_mbedtls_socket_set_handshake_timeout(ebisu_mbedtls_socket_t* sock,
        ebisu_system_t* system, unsigned int seconds);

/**
 * \brief Sets the clock the server's certificate is checked at the time of.
 *
 * While the clock has only a lower bound of the time, a certificate that
 * expired before it is refused, and one not valid yet is not, as it may have
 * been issued since. A certificate not valid yet at a time the clock had is
 * refused, and the clock's time forgotten, so that the next connection is
 * checked at a lower bound and its response gives the clock the time again:
 * more likely than a certificate from the future, the time fell behind. See
 * ebisu_clock_forget_time(). The clock keeps its time if the certificate is
 * refused for anything else too, as one that the CA certificates don't lead to
 * or that is for another host: such a certificate says nothing of the time.
 *
 * \param [out] sock The socket.
 * \param [in] clock The clock, which has to outlive the socket, or NULL, as the
 * socket starts, for the system's time.
 */
void ebisu_mbedtls_socket_set_clock(ebisu_mbedtls_socket_t* sock, ebisu_clock_t* clock);

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
void ebisu_mbedtls_socket_set_logger(ebisu_mbedtls_socket_t* sock, ebisu_logger_t* logger);

/**
 * \brief The TLS version the connection open negotiated.
 *
 * As mbedTLS names it, with mbedtls_ssl_get_version(): "TLSv1.2" or
 * "TLSv1.3".
 *
 * \param [in] sock The socket.
 * \return The version, which lasts as long as the connection does, or NULL
 * while there is none: before a connect, after one that failed, and once
 * closed.
 */
const char* ebisu_mbedtls_socket_get_protocol(const ebisu_mbedtls_socket_t* sock);

/**
 * \brief The cipher suite the connection open negotiated.
 *
 * As mbedTLS names it, with mbedtls_ssl_get_ciphersuite(): the standard name
 * with dashes, such as "TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256" or
 * "TLS1-3-AES-128-GCM-SHA256".
 *
 * \param [in] sock The socket.
 * \return The suite, which lasts as long as the connection does, or NULL
 * while there is none.
 */
const char* ebisu_mbedtls_socket_get_cipher(const ebisu_mbedtls_socket_t* sock);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* EBISU_MBEDTLS_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
