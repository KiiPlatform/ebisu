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
