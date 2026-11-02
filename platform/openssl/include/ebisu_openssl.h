/**
 * @file ebisu_openssl.h
 * @brief A socket of OpenSSL over BSD sockets.
 */
#ifndef _EBISU_OPENSSL
#define _EBISU_OPENSSL

#include <ebisu_clock.h>
#include <ebisu_socket.h>
#include <openssl/ssl.h>
#include <stdbool.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief A TLS connection over a blocking BSD socket, with OpenSSL.
 *
 * It verifies the server's certificate chain against the system's CA
 * certificates, and the host name, at the time of the clock set by
 * ebisu_openssl_socket_set_clock(), or the system's without one, against the
 * CA certificates of the configuration set by ebisu_openssl_socket_set_tls(),
 * or the system's without one. A receive that times out is EBISU_SOCK_FAIL,
 * which recv_timed_out tells from a lost connection. One serves one connection
 * at a time, and is used again for the next once closed. OpenSSL 1.0.2 and
 * later, initialized and made thread-safe on first use.
 */
typedef struct ebisu_openssl_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    SSL *ssl; /**< \private */
    /** \private The configuration connections are made with, or NULL for the
     * one with the system's CA certificates. */
    struct ebisu_openssl_tls_t* tls;
    int socket; /**< \private */
    unsigned int to_recv; /**< \private */
    unsigned int to_send; /**< \private */
    /** \private Bounds looking up the host, and each attempt to connect to
     * one of its addresses, in seconds, 0 for none. */
    unsigned int to_connect;
    /** \private SSL_get_error() and errno after the last SSL_read() that read
     * nothing, for recv_timed_out. */
    int recv_ssl_error;
    int recv_errno; /**< \private */
    /** \private Whether SSL_read() or SSL_write() ended the connection with an
     * error, after which SSL_shutdown() must not be called. */
    bool ssl_failed;
    /** \private The time the server's certificate is checked at, or NULL for
     * the system's. */
    ebisu_clock_t* clock;
    /** \private Whether the clock had the time, rather than its floor only,
     * when the connection was made. */
    bool time_exact;
} ebisu_openssl_socket_t;

/**
 * \brief The TLS configuration connections share.
 *
 * One SSL_CTX, with the CA certificates servers' certificates have to chain
 * to, set up once rather than for each connection. Sockets of any number of
 * tasks may share one.
 */
typedef struct ebisu_openssl_tls_t {
    SSL_CTX* ssl_ctx; /**< \private */
} ebisu_openssl_tls_t;

/**
 * \brief Sets up a TLS configuration.
 *
 * For a device without the system's CA certificates, or with them where
 * OpenSSL doesn't look, give the CA certificates the servers' chain to.
 * Without either, the system's are used, from where OpenSSL was built to find
 * them, as when no configuration is set.
 *
 * \param [out] tls The configuration.
 * \param [in] ca_file File of PEM CA certificates, or NULL.
 * \param [in] ca_path Directory of CA certificates named by their hash, as
 * openssl rehash or c_rehash names them, or NULL.
 * \return true, or false, having printed why, if the certificates couldn't be
 * loaded or OpenSSL failed.
 */
bool ebisu_openssl_tls_init(ebisu_openssl_tls_t* tls, const char* ca_file, const char* ca_path);

/**
 * \brief Frees a TLS configuration.
 *
 * Connections still open keep what they need of it, but no socket may connect
 * with it any more.
 *
 * \param [in,out] tls The configuration.
 */
void ebisu_openssl_tls_free(ebisu_openssl_tls_t* tls);

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. It bounds looking
 * up the host and each attempt to connect too, unless
 * ebisu_openssl_socket_set_connect_timeout() sets another.
 */
void ebisu_openssl_socket_init(
        ebisu_openssl_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

/**
 * \brief Sets how long connecting may take.
 *
 * The host is looked up with getaddrinfo(), and its addresses tried in the
 * order it gives them, IPv6 and IPv4, until one connects. The timeout bounds
 * the lookup, and each attempt to connect to an address, but not the TLS
 * handshake, which the receive and send timeouts bound.
 *
 * \param [out] sock The socket.
 * \param [in] seconds 0 for no bound: the lookup takes as long as the system's
 * resolver does, and an attempt as long as the system's TCP does.
 */
void ebisu_openssl_socket_set_connect_timeout(ebisu_openssl_socket_t* sock, unsigned int seconds);

/**
 * \brief Sets the TLS configuration the socket connects with.
 *
 * \param [out] sock The socket.
 * \param [in] tls The configuration, which has to outlive the socket's
 * connections, or NULL for one with the system's CA certificates, which every
 * socket without one of its own shares.
 */
void ebisu_openssl_socket_set_tls(ebisu_openssl_socket_t* sock, ebisu_openssl_tls_t* tls);

/**
 * \brief Sets the clock the server's certificate is checked at the time of.
 *
 * See ebisu_clock_t for how a certificate is checked while the clock has only
 * a lower bound of the time.
 *
 * \param [out] sock The socket.
 * \param [in] clock The clock, or NULL for the system's time.
 */
void ebisu_openssl_socket_set_clock(ebisu_openssl_socket_t* sock, ebisu_clock_t* clock);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_OPENSSL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
