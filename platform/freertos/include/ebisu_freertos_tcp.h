/**
 * @file ebisu_freertos_tcp.h
 * @brief A socket of plain TCP over FreeRTOS-Plus-TCP.
 */
#ifndef _EBISU_FREERTOS_TCP
#define _EBISU_FREERTOS_TCP

#include <ebisu_log.h>
#include <ebisu_socket.h>

#include <FreeRTOS.h>
#include <FreeRTOS_IP.h>
#include <FreeRTOS_Sockets.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief A TCP connection over FreeRTOS-Plus-TCP, without TLS.
 *
 * For TLS over it, with ebisu_mbedtls_socket_t, whose records it sends and
 * receives, or for a server that takes plain TCP, such as the MQTT broker's
 * port without TLS, which kii_enable_insecure_mqtt() connects to. Nothing it
 * sends or receives itself is encrypted or authenticated.
 *
 * It connects to an IPv4 address as written, without a lookup, or looks a
 * name up with FreeRTOS_getaddrinfo(), and tries each of its addresses in
 * turn. With ipconfigUSE_IPv6, it looks the host's IPv6 addresses up too, if
 * none of its IPv4 ones connects. A lookup takes as long as
 * FreeRTOS-Plus-TCP's DNS client does, as ipconfigDNS_REQUEST_ATTEMPTS and
 * ipconfigDNS_RECEIVE_BLOCK_TIME_TICKS have it, and each attempt to connect
 * the connect timeout at the most: see
 * ebisu_freertos_tcp_socket_set_connect_timeout().
 *
 * A receive that times out is EBISU_SOCK_FAIL, which recv_timed_out tells
 * from a lost connection, and leaves the connection as it was. A peer that
 * closes or resets the connection is the end of the stream, which
 * FreeRTOS-Plus-TCP doesn't tell apart. recv_wait waits with a receive that
 * peeks. A close shuts the connection down, and waits up to 2 seconds for the
 * peer to close its side.
 *
 * Why an operation failed, its last_error says: a host that didn't resolve,
 * with FreeRTOS_getaddrinfo()'s error, an address that refused, with
 * FreeRTOS-Plus-TCP's error, negated to a pdFREERTOS_ERRNO_* value, a
 * timeout, a peer that closed, or another failure, with FreeRTOS-Plus-TCP's
 * error. It also goes to the logger set by
 * ebisu_freertos_tcp_socket_set_logger(), if any.
 *
 * One serves one connection at a time, from one task at a time, and is used
 * again for the next once closed. Needs ipconfigUSE_TCP and ipconfigUSE_DNS.
 * Initialize with ebisu_freertos_tcp_socket_init().
 */
typedef struct ebisu_freertos_tcp_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    Socket_t _socket; /**< \private */
    unsigned int _to_recv; /**< \private */
    unsigned int _to_send; /**< \private */
    unsigned int _to_connect; /**< \private */
    /** \private Whether the last receive failed for its timeout. */
    bool _recv_timed_out;
    /** \private Where diagnostics go, or NULL for nowhere. */
    ebisu_logger_t* _logger;
    /** \private Why the last operation that returned EBISU_SOCK_FAIL failed,
     * for last_error, and more of it. */
    ebisu_sock_error_t _error;
    long _error_detail; /**< \private */
} ebisu_freertos_tcp_socket_t;

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. It bounds each
 * attempt to connect too, unless ebisu_freertos_tcp_socket_set_connect_timeout()
 * sets another.
 */
void ebisu_freertos_tcp_socket_init(
        ebisu_freertos_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

/**
 * \brief Sets how long each attempt to connect to one of the host's addresses
 * may take.
 *
 * Not the lookup, which FreeRTOS-Plus-TCP's own settings bound.
 *
 * \param [out] sock The socket.
 * \param [in] seconds 0 for no bound, which with INCLUDE_vTaskSuspend waits
 * for as long as FreeRTOS-Plus-TCP tries.
 */
void ebisu_freertos_tcp_socket_set_connect_timeout(ebisu_freertos_tcp_socket_t* sock,
        unsigned int seconds);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Why an operation failed, at EBISU_LOG_ERROR: a host that didn't resolve or
 * connect, a send or a receive that failed. A receive that times out isn't
 * one.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_freertos_tcp_socket_set_logger(ebisu_freertos_tcp_socket_t* sock,
        ebisu_logger_t* logger);

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_FREERTOS_TCP */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
