/** \file ebisu_socket.h
 * Provides socket abstraction.
 */
#ifndef _EBISU_SOCKET
#define _EBISU_SOCKET

#include "ebisu_object.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif


#ifdef __cplusplus
extern "C" {
#endif

/** \brief Indicates socket callback result */
typedef enum ebisu_sock_code_t {
    /** \brief Return this code when operation succeed. */
    EBISU_SOCK_OK,

    /** \brief Return this code when operation failed. */
    EBISU_SOCK_FAIL,

    /** \brief Return this code when operation is in progress.
     *
     * This code is used when the underlying socket is non-blocking mode.
     * The operation is called again with the same arguments
     * until it returns EBISU_SOCK_OK or EBISU_SOCK_FAIL.
     */
    EBISU_SOCK_AGAIN
} ebisu_sock_code_t;

/** \brief Why a socket operation failed, as the socket can tell. See
 * ebisu_socket_ops_t::last_error. */
typedef enum ebisu_sock_error_t {
    /** \brief Nothing failed, or the socket can't tell why. */
    EBISU_SOCK_ERROR_NONE,
    /** \brief The host name didn't resolve. The detail is the resolver's
     * error, such as getaddrinfo()'s. */
    EBISU_SOCK_ERROR_DNS,
    /** \brief None of the host's addresses took the connection. The detail
     * is the system's error, as errno has it. */
    EBISU_SOCK_ERROR_CONNECT,
    /** \brief TLS failed, in the handshake or after it, for another reason
     * than the server's certificate. The detail is the TLS library's error. */
    EBISU_SOCK_ERROR_TLS,
    /** \brief The server's certificate was refused. The detail is why, as
     * the TLS library says it, such as OpenSSL's X509_V_ERR_* codes. */
    EBISU_SOCK_ERROR_VERIFY,
    /** \brief A timeout ran out: looking the host up, connecting, sending or
     * receiving. */
    EBISU_SOCK_ERROR_TIMEOUT,
    /** \brief The peer closed or reset the connection. */
    EBISU_SOCK_ERROR_CLOSED,
    /** \brief Another failure. The detail is the system's error, if there is
     * one. */
    EBISU_SOCK_ERROR_OTHER
} ebisu_sock_error_t;

/** \brief A socket. See struct ebisu_socket_t. */
typedef struct ebisu_socket_t ebisu_socket_t;

/** \brief What a socket does.
 *
 * Applications implement these for the target environment, doing TLS inside
 * them unless the connection is insecure. See ebisu_object.h for how self
 * leads to the socket's state.
 */
typedef struct ebisu_socket_ops_t {
    /** \brief Connects to the server.
     *
     * \param [in] self The socket.
     * \param [in] host Host name.
     * \param [in] port TCP port number.
     */
    ebisu_sock_code_t (*connect)(ebisu_socket_t* self, const char* host, uint16_t port);

    /** \brief Sends data to the server.
     *
     * Called repeatedly until the whole request is sent: it may send fewer
     * bytes than asked.
     *
     * \param [in] self The socket.
     * \param [in] buffer Data to send.
     * \param [in] length Length of buffer.
     * \param [out] out_sent_length How many bytes were sent.
     */
    ebisu_sock_code_t (*send)(ebisu_socket_t* self, const char* buffer, size_t length,
            size_t* out_sent_length);

    /** \brief Receives data from the server.
     *
     * Called repeatedly until out_actual_length is set to 0, which, with
     * EBISU_SOCK_OK, means the server closed the connection.
     *
     * \param [in] self The socket.
     * \param [out] buffer Where data read from the socket must be written.
     * \param [in] length_to_read Maximum size requested to read.
     * \param [out] out_actual_length How many bytes were read.
     */
    ebisu_sock_code_t (*recv)(ebisu_socket_t* self, char* buffer, size_t length_to_read,
            size_t* out_actual_length);

    /** \brief Closes the socket.
     *
     * \param [in] self The socket.
     */
    ebisu_sock_code_t (*close)(ebisu_socket_t* self);

    /** \brief Whether the last receive failed because its timeout ran out.
     *
     * Optional, and asked only by the MQTT task. Its recv returns
     * EBISU_SOCK_FAIL both when its receive timeout runs out with nothing
     * received, which is routine on an idle connection, and when the
     * connection is lost. This tells the two apart. It is called right after
     * such an EBISU_SOCK_FAIL, before any other operation on the socket. See
     * kii_set_mqtt_socket() for what the task does with the answer, and
     * without one.
     *
     * \param [in] self The socket.
     * \return true if the receive failed because its timeout ran out, false if
     * it failed for any other reason.
     */
    bool (*recv_timed_out)(ebisu_socket_t* self);

    /** \brief Why the last operation that returned EBISU_SOCK_FAIL failed.
     *
     * Optional. khc asks it right after such a failure of connect, send or
     * recv, before closing the socket, and khc_get_sock_error() gives the
     * answer. An application can ask its MQTT socket from the callback set by
     * kii_set_cb_mqtt_event().
     *
     * \param [in] self The socket.
     * \param [out] out_detail More of it, as each ebisu_sock_error_t says, or 0.
     * \return Why, or EBISU_SOCK_ERROR_NONE if the socket can't tell.
     */
    ebisu_sock_error_t (*last_error)(ebisu_socket_t* self, long* out_detail);

    /** \brief Waits for recv to have something to return.
     *
     * Optional, and asked only by the MQTT task, when kii_mqtt_publish() may
     * give it a message to send: it waits for the next packet a little at a
     * time, rather than in a recv that can last the receive timeout, so that a
     * message given meanwhile is sent soon. Without it, the task waits in recv.
     * It is called from the task that calls the other operations, never at
     * the same time as one.
     *
     * It returns once recv would return without waiting: with data, counting
     * what the socket has received and not yet returned, such as decrypted
     * TLS data, with the end of the stream, or with a failure. Waiting must not
     * change what recv then returns. It may return true for what turns out to
     * be no data, such as a TLS record without any, or the start of a record:
     * recv then waits, as it would have.
     *
     * \param [in] self The socket.
     * \param [in] timeout_ms The longest to wait, in milliseconds.
     * \return true once recv would return without waiting, false if timeout_ms
     * ran out first.
     */
    bool (*recv_wait)(ebisu_socket_t* self, uint32_t timeout_ms);
} ebisu_socket_ops_t;

/** \brief A socket.
 *
 * Embed it in the struct holding the socket's state, as ebisu_object.h shows.
 */
struct ebisu_socket_t {
    const ebisu_socket_ops_t* ops; /**< \brief What the socket does. */
};

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_SOCKET */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
