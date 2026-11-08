/**
 * @file ebisu_schannel.h
 * @brief A socket of Schannel over Winsock.
 */
#ifndef _EBISU_SCHANNEL
#define _EBISU_SCHANNEL

#include <winsock2.h>
#ifndef SECURITY_WIN32
#define SECURITY_WIN32
#endif
#include <security.h>

#include <ebisu_clock.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief A TLS connection over a Winsock socket, with Schannel through SSPI.
 *
 * Schannel negotiates the protocol versions the system enables, TLS 1.2 at
 * most, and the socket checks the server's certificate itself, with crypt32: a
 * chain to a root in the Windows root store, valid for server authentication,
 * issued for the host, and in date at the time of the clock set by
 * ebisu_schannel_socket_set_clock(), or the system's without one. Revocation
 * isn't checked. A receive that times out is EBISU_SOCK_FAIL, which
 * recv_timed_out tells from a lost connection, and leaves the connection as it
 * was. One serves one connection at a time, and is used again for the next once
 * closed. Why an operation failed, its last_error says: a host that didn't
 * resolve, with getaddrinfo()'s error, an address that refused, with the
 * Winsock error, a certificate refused, with the CERT_E_* or TRUST_E_* code, a
 * TLS failure, with the SSPI status, a timeout, or a peer that closed or reset
 * the connection, with the Winsock error if there is one. It also goes to the
 * logger set by ebisu_schannel_socket_set_logger(), if any.
 * Winsock has to have been started, with WSAStartup(), before the first
 * connection.
 */
typedef struct ebisu_schannel_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    /* Receive and send timeouts in seconds; 0 waits for ever. */
    unsigned int to_recv; /**< \private */
    unsigned int to_send; /**< \private */
    /* The time the server's certificate is checked at, or NULL for the
       system's. See ebisu_clock_t. */
    ebisu_clock_t* clock; /**< \private */

    /* The server's name, which its certificate must be issued for. */
    char host[256]; /**< \private */
    SOCKET socket; /**< \private */
    CredHandle cred; /**< \private */
    int has_cred; /**< \private */
    CtxtHandle ctxt; /**< \private */
    int has_ctxt; /**< \private */
    SecPkgContext_StreamSizes sizes; /**< \private */

    /* What has been received from the peer and not yet returned. recv_buf
       holds TLS records as they came off the socket, recv_len bytes of them.
       DecryptMessage() decrypts a record in place, so once one has been
       decrypted its plaintext, the plain_len bytes at plain, lies inside the
       first record_len bytes of recv_buf, and anything after those is the
       start of the next record. */
    char* recv_buf; /**< \private */
    size_t recv_len; /**< \private */
    size_t record_len; /**< \private */
    char* plain; /**< \private */
    size_t plain_len; /**< \private */
    /* The peer ended the stream: TLS close_notify, or the end of the TCP
       stream at a record boundary. */
    int peer_closed; /**< \private */
    /* Whether the last receive failed because nothing arrived within
       to_recv, for recv_timed_out. */
    int recv_timed_out; /**< \private */
    /* Where diagnostics go, or NULL for nowhere. */
    ebisu_logger_t* logger; /**< \private */
    /* Why the last operation that returned EBISU_SOCK_FAIL failed, for
       last_error, and more of it. */
    ebisu_sock_error_t error; /**< \private */
    long error_detail; /**< \private */

    /* A record being encrypted: header, at most cbMaximumMessage bytes of
       data, and trailer. */
    char* send_buf; /**< \private */
} ebisu_schannel_socket_t;

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none.
 */
void ebisu_schannel_socket_init(
        ebisu_schannel_socket_t* sock,
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
void ebisu_schannel_socket_set_clock(ebisu_schannel_socket_t* sock, ebisu_clock_t* clock);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Why an operation failed, at EBISU_LOG_ERROR: a host that didn't resolve or
 * connect, a certificate refused and why, Schannel's or Winsock's error, with
 * the system's text for it. A receive that times out isn't one.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_schannel_socket_set_logger(ebisu_schannel_socket_t* sock, ebisu_logger_t* logger);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_SCHANNEL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
