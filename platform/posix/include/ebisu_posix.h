/**
 * @file ebisu_posix.h
 * @brief The system, its clock, a lock, a socket of plain TCP and the name
 * servers on POSIX.
 */
#ifndef _EBISU_POSIX
#define _EBISU_POSIX

#include <ebisu_dns.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>

#include <pthread.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The system on POSIX.
 *
 * Each task is a detached thread, which goes by itself when it ends. A delay is
 * nanosleep(), resumed through signals until the whole time has passed. The
 * monotonic clock is CLOCK_MONOTONIC. Its locks, from
 * ebisu_system_lock_create(), are ebisu_posix_lock_t's from the heap. Its
 * clock, from ebisu_system_clock(), is the time of day, CLOCK_REALTIME, as
 * ebisu_clock_from_system_time() takes it: earlier than the library's source,
 * a time the system has not set, it is a lower bound. It takes no time from
 * servers, and has nothing to doubt: the time is what sets the system's, NTP
 * for instance, keeps. The system has no state, so this one serves everything.
 *
 * \return The system.
 */
ebisu_system_t* ebisu_posix_system(void);

/** \brief A lock that is a pthread mutex. */
typedef struct ebisu_posix_lock_t {
    ebisu_lock_t base; /**< \brief What ebisu is given. */
    pthread_mutex_t mutex; /**< \private */
} ebisu_posix_lock_t;

/**
 * \brief Initializes the mutex, and makes base a lock of it.
 *
 * \param [out] lock The lock.
 * \return 0, or what pthread_mutex_init() returned on failure.
 */
int ebisu_posix_lock_init(ebisu_posix_lock_t* lock);

/**
 * \brief Destroys the mutex, which nothing may hold or use any more.
 *
 * \param [in,out] lock The lock.
 */
void ebisu_posix_lock_destroy(ebisu_posix_lock_t* lock);

/**
 * \brief A TCP connection over a blocking BSD socket, without TLS.
 *
 * For a server that takes plain TCP, such as the MQTT broker's port without
 * TLS, which kii_enable_insecure_mqtt() connects to. Nothing it sends or
 * receives is encrypted or authenticated: anyone on the path can read it, and
 * change it.
 *
 * It connects as ebisu_openssl_socket_t does, with the same code, but for the
 * TLS: see ebisu_posix_tcp_socket_set_connect_timeout(). A receive that times
 * out is EBISU_SOCK_FAIL, which recv_timed_out tells from a lost connection,
 * and a peer that closes the connection is the end of the stream. A send to a
 * peer that has gone fails, and never raises SIGPIPE. One serves one
 * connection at a time, and is used again for the next once closed. Why an
 * operation failed, its last_error says: a host that didn't resolve, with
 * getaddrinfo()'s error, an address that refused, with errno, a timeout, a
 * peer that closed or reset the connection, or another failure, with errno.
 * It also goes to the logger set by ebisu_posix_tcp_socket_set_logger(), if
 * any.
 *
 * Initialize with ebisu_posix_tcp_socket_init().
 */
typedef struct ebisu_posix_tcp_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    int socket; /**< \private */
    unsigned int to_recv; /**< \private */
    unsigned int to_send; /**< \private */
    /** \private Bounds looking up the host, and each attempt to connect to
     * one of its addresses, in seconds, 0 for none. */
    unsigned int to_connect;
    /** \private errno after the last recv() that failed, for
     * recv_timed_out. */
    int recv_errno;
    /** \private Where diagnostics go, or NULL for nowhere. */
    ebisu_logger_t* logger;
    /** \private Why the last operation that returned EBISU_SOCK_FAIL failed,
     * for last_error, and more of it. */
    ebisu_sock_error_t error;
    long error_detail; /**< \private */
    /** \private What ebisu_posix_tcp_socket_set_keepalive() sets: the idle
     * time, 0 for no keepalive, the interval and the count. */
    unsigned int keepalive_idle;
    unsigned int keepalive_interval; /**< \private */
    unsigned int keepalive_count; /**< \private */
    /** \private What ebisu_posix_tcp_socket_set_nodelay() sets. */
    bool nodelay;
} ebisu_posix_tcp_socket_t;

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. It bounds looking
 * up the host and each attempt to connect too, unless
 * ebisu_posix_tcp_socket_set_connect_timeout() sets another.
 */
void ebisu_posix_tcp_socket_init(
        ebisu_posix_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

/**
 * \brief Sets how long connecting may take.
 *
 * The host is looked up with getaddrinfo(), and its addresses tried in the
 * order it gives them, IPv6 and IPv4, until one connects. The timeout bounds
 * the lookup, and each attempt to connect to an address.
 *
 * getaddrinfo() can't be given a timeout: a lookup the timeout cuts short goes
 * on, on a thread of its own, for as long as the system's resolver takes. A
 * connect to the same host and port meanwhile, of any of these sockets, waits
 * for that lookup, within its own timeout, rather than start another: lookups
 * that don't end leave a thread for each host and port, however often
 * connects time out on them.
 *
 * \param [out] sock The socket.
 * \param [in] seconds 0 for no bound: the lookup takes as long as the system's
 * resolver does, and an attempt as long as the system's TCP does.
 */
void ebisu_posix_tcp_socket_set_connect_timeout(ebisu_posix_tcp_socket_t* sock,
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
void ebisu_posix_tcp_socket_set_logger(ebisu_posix_tcp_socket_t* sock,
        ebisu_logger_t* logger);

/**
 * \brief Sets whether TCP checks an idle connection is still there.
 *
 * With keepalive, TCP probes a connection that has been idle for idle_s
 * seconds, and gives the connection up when count probes, interval_s seconds
 * apart, go unanswered: a send or a receive on it then fails, rather than
 * the next request finding out. So a peer that lost power, or a NAT that
 * dropped the connection's entry, is noticed within about
 * idle_s + interval_s * count seconds of the connection going idle.
 *
 * It applies from the next connect: SO_KEEPALIVE, with TCP_KEEPIDLE
 * (TCP_KEEPALIVE on macOS), TCP_KEEPINTVL and TCP_KEEPCNT where the system
 * has them. One the system hasn't, or a value it refuses, as Linux refuses an
 * idle time or an interval over 32767 seconds and a count over 127, is said at
 * EBISU_LOG_WARNING to the logger, and the connection made without it.
 *
 * \param [out] sock The socket.
 * \param [in] idle_s Seconds idle before the first probe, or 0 for no
 * keepalive, as the socket starts.
 * \param [in] interval_s Seconds between probes, or 0 for the system's.
 * \param [in] count Probes unanswered before the connection is given up, or
 * 0 for the system's.
 */
void ebisu_posix_tcp_socket_set_keepalive(ebisu_posix_tcp_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count);

/**
 * \brief Sets whether what is sent goes at once, with TCP_NODELAY.
 *
 * Without it, as the socket starts, Nagle's algorithm holds a small segment
 * back while an earlier one is unacknowledged: a request sent in two pieces
 * can wait for the first's ACK, which a peer may delay. It applies from the
 * next connect. A system that refuses it is said at EBISU_LOG_WARNING.
 *
 * \param [out] sock The socket.
 * \param [in] nodelay true to send at once.
 */
void ebisu_posix_tcp_socket_set_nodelay(ebisu_posix_tcp_socket_t* sock, bool nodelay);

/**
 * \brief The name servers on POSIX, for platform's DNS probe.
 *
 * The servers are those of the nameserver lines of /etc/resolv.conf whose
 * address is one, in order, on port 53, as the C libraries take them: an IPv6
 * address's scope is read as ebisu_dns_parse_address() reads it, an
 * interface's name by if_nametoindex(), and a line with another scope is taken
 * without it where the port is built with glibc, and skipped where it isn't,
 * as glibc and musl do. So are the lines read: with glibc, split at spaces and
 * tabs, and whole, however long, and otherwise split at what isspace() takes,
 * a line longer than 254 bytes skipped. An address ends at a blank: a '#' or a
 * ';' right after it makes it one no longer. Without a server, or without the
 * file, it is 127.0.0.1, as for the C libraries.
 *
 * A server is asked over a UDP socket connected to it, so that its port
 * unreachable is told. A scope is said as its interface's name, by
 * if_indextoname(), and no server answering is EAI_AGAIN, as for
 * getaddrinfo(). They have no state, so this serves everything.
 *
 * \return The name servers.
 */
ebisu_dns_t* ebisu_posix_dns(void);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_POSIX */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
