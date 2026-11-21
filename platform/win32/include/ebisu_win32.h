/**
 * @file ebisu_win32.h
 * @brief The system, its clock, a lock, a socket of plain TCP and the name
 * servers on Windows.
 */
#ifndef EBISU_WIN32_H
#define EBISU_WIN32_H

#include <ebisu_dns.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>

#include <windows.h>

#include <stdbool.h>
#include <stdint.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The system on Windows.
 *
 * Each task runs on a thread of its own, made with _beginthreadex(), which
 * nothing waits for. A delay is Sleep(). The monotonic clock is
 * GetTickCount64(), which doesn't jump when the wall clock is set. Its locks,
 * from ebisu_system_lock_create(), are ebisu_win32_lock_t's from the heap. Its
 * clock, from ebisu_system_clock(), is the time of day,
 * GetSystemTimeAsFileTime(), as ebisu_clock_from_system_time() takes it. It
 * takes no time from servers, and has nothing to doubt: the time is what
 * Windows Time keeps. The system has no state, so this one serves everything.
 *
 * \return The system.
 */
ebisu_system_t* ebisu_win32_system(void);

/** \brief A lock that is an SRWLOCK, which needs no destroying. */
typedef struct ebisu_win32_lock_t {
    ebisu_lock_t base; /**< \brief What ebisu is given. */
    SRWLOCK srwlock; /**< \private */
} ebisu_win32_lock_t;

/**
 * \brief Initializes the SRWLOCK, and makes base a lock of it.
 *
 * \param [out] lock The lock.
 */
void ebisu_win32_lock_init(ebisu_win32_lock_t* lock);

/**
 * \brief A TCP connection over a blocking Winsock 2 socket, without TLS.
 *
 * For a server that takes plain TCP, such as the MQTT broker's port without
 * TLS, which kii_enable_insecure_mqtt() connects to. Nothing it sends or
 * receives is encrypted or authenticated: anyone on the path can read it, and
 * change it.
 *
 * It connects as ebisu_schannel_socket_t does, with the same code, but for the
 * TLS: see ebisu_win32_tcp_socket_set_connect_timeout(). A receive waits for
 * data with select(), for no longer than its timeout, and only then reads it,
 * so that a receive that times out leaves the socket as it was, as Winsock's
 * own receive timeout wouldn't. It is EBISU_SOCK_FAIL, which recv_timed_out
 * tells from a lost connection, and a peer that closes the connection is the
 * end of the stream. One serves one connection at a time, and is used again
 * for the next once closed. Why an operation failed, its last_error says: a
 * host that didn't resolve, with GetAddrInfoExW()'s error, an address that
 * refused, with the Winsock error, a timeout, a peer that closed or reset the
 * connection, or another failure, with the Winsock error. It also goes to the
 * logger set by ebisu_win32_tcp_socket_set_logger(), if any.
 *
 * Winsock has to have been started, with WSAStartup(), before the first
 * connect, and the library links ws2_32.
 *
 * Initialize with ebisu_win32_tcp_socket_init().
 */
typedef struct ebisu_win32_tcp_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    /** \private The SOCKET, INVALID_SOCKET while there is no connection. */
    uintptr_t socket;
    unsigned int to_recv; /**< \private */
    unsigned int to_send; /**< \private */
    /** \private Bounds looking up the host, and each attempt to connect to
     * one of its addresses, in seconds, 0 for none. */
    unsigned int to_connect;
    /** \private Whether the last receive that failed did so because its
     * timeout ran out, for recv_timed_out. */
    bool recv_timed_out;
    /** \private Where diagnostics go, or NULL for nowhere. */
    ebisu_logger_t* logger;
    /** \private Why the last operation that returned EBISU_SOCK_FAIL failed,
     * for last_error, and more of it. */
    ebisu_sock_error_t error;
    long error_detail; /**< \private */
    /** \private What ebisu_win32_tcp_socket_set_keepalive() sets: the idle
     * time, 0 for no keepalive, the interval and the count. */
    unsigned int keepalive_idle;
    unsigned int keepalive_interval; /**< \private */
    unsigned int keepalive_count; /**< \private */
    /** \private What ebisu_win32_tcp_socket_set_nodelay() sets. */
    bool nodelay;
} ebisu_win32_tcp_socket_t;

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. It bounds looking
 * up the host and each attempt to connect too, unless
 * ebisu_win32_tcp_socket_set_connect_timeout() sets another.
 */
void ebisu_win32_tcp_socket_init(
        ebisu_win32_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

/**
 * \brief Sets how long connecting may take.
 *
 * The host is looked up with GetAddrInfoExW(), and its addresses tried in the
 * order it gives them, IPv6 and IPv4, until one connects. The timeout bounds
 * the lookup, which is cancelled when it runs out, and each attempt to connect
 * to an address.
 *
 * \param [out] sock The socket.
 * \param [in] seconds 0 for no bound: the lookup takes as long as the system's
 * resolver does, and an attempt as long as the system's TCP does.
 */
void ebisu_win32_tcp_socket_set_connect_timeout(ebisu_win32_tcp_socket_t* sock,
        unsigned int seconds);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Why an operation failed, at EBISU_LOG_ERROR: a host that didn't resolve or
 * connect, a send or a receive that failed, with the Winsock error and the
 * system's text for it. A receive that times out isn't one.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_win32_tcp_socket_set_logger(ebisu_win32_tcp_socket_t* sock,
        ebisu_logger_t* logger);

/**
 * \brief Sets whether TCP checks an idle connection is still there.
 *
 * As the POSIX port's ebisu_posix_tcp_socket_set_keepalive() does: TCP probes
 * a connection idle for idle_s seconds, and gives it up when count probes,
 * interval_s seconds apart, go unanswered, so that a peer that has gone is
 * noticed within about idle_s + interval_s * count seconds of the connection
 * going idle.
 *
 * It applies from the next connect: SO_KEEPALIVE, with TCP_KEEPIDLE,
 * TCP_KEEPINTVL and TCP_KEEPCNT, which Windows takes from Windows 10, version
 * 1709. One it refuses, as an earlier Windows does, is said with the Winsock
 * error at EBISU_LOG_WARNING to the logger, and the connection made without
 * it.
 *
 * \param [out] sock The socket.
 * \param [in] idle_s Seconds idle before the first probe, or 0 for no
 * keepalive, as the socket starts.
 * \param [in] interval_s Seconds between probes, or 0 for the system's.
 * \param [in] count Probes unanswered before the connection is given up, or
 * 0 for the system's.
 */
void ebisu_win32_tcp_socket_set_keepalive(ebisu_win32_tcp_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count);

/**
 * \brief Sets whether what is sent goes at once, with TCP_NODELAY.
 *
 * Without it, as the socket starts, Nagle's algorithm holds a small segment
 * back while an earlier one is unacknowledged: a request sent in two pieces
 * can wait for the first's ACK, which a peer may delay. It applies from the
 * next connect.
 *
 * \param [out] sock The socket.
 * \param [in] nodelay true to send at once.
 */
void ebisu_win32_tcp_socket_set_nodelay(ebisu_win32_tcp_socket_t* sock, bool nodelay);

/**
 * \brief The name servers on Windows, for platform's DNS probe.
 *
 * The servers are those of the network adapters that are up, as
 * GetAdaptersAddresses() gives them, in its order: IPv4 and IPv6, with an IPv6
 * one's scope, but one an earlier adapter has too, and the placeholders
 * fec0:0:0:ffff::1 to ::3, which Windows lists for an adapter without an IPv6
 * server and which nothing answers on. Without any, there are none.
 *
 * A server is asked over a UDP socket of Winsock 2 connected to it, so that
 * its port unreachable is told, as Winsock reports it, with WSAECONNRESET. A
 * scope's name is read by if_nametoindex() and said by if_indextoname(), and
 * no server answering is WSATRY_AGAIN, getaddrinfo()'s EAI_AGAIN. Winsock has
 * to have been started, with WSAStartup(), and the library links ws2_32 and
 * iphlpapi. They have no state, so this serves everything.
 *
 * \return The name servers.
 */
ebisu_dns_t* ebisu_win32_dns(void);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* EBISU_WIN32_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
