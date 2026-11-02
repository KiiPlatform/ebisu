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
 * ebisu_openssl_socket_set_clock(), or the system's without one. A receive
 * that times out is EBISU_SOCK_FAIL, which recv_timed_out tells from a lost
 * connection. One serves one connection at a time, and is used again for the
 * next once closed. OpenSSL 1.0.2 and later, initialized and made thread-safe
 * on first use.
 */
typedef struct ebisu_openssl_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    SSL *ssl; /**< \private */
    SSL_CTX *ssl_ctx; /**< \private */
    int socket; /**< \private */
    unsigned int to_recv; /**< \private */
    unsigned int to_send; /**< \private */
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
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. On Linux it bounds
 * connecting too.
 */
void ebisu_openssl_socket_init(
        ebisu_openssl_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

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
