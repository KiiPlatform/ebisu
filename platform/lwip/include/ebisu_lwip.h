/**
 * @file ebisu_lwip.h
 * @brief A socket of plain TCP over lwIP's sockets, and its name servers.
 */
#ifndef EBISU_LWIP_H
#define EBISU_LWIP_H

#include <ebisu_dns.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#if defined(EBISU_TLS_RESUME)
#include <ebisu_system.h>
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief A TCP connection over lwIP's sockets, without TLS.
 *
 * For TLS over it, with ebisu_mbedtls_socket_t, whose records it sends and
 * receives, or for a server that takes plain TCP, such as the MQTT broker's
 * port without TLS, which kii_enable_insecure_mqtt() connects to. Nothing it
 * sends or receives itself is encrypted or authenticated.
 *
 * It looks the host up with lwip_getaddrinfo(), which takes an address as
 * written as it is, and gives a name's first address only, of the family
 * LWIP_DNS_ADDRTYPE_DEFAULT puts first, IPv4 in lwIP 1.4, which has no other:
 * that one address is what it connects to. A lookup takes as long as lwIP's DNS
 * client does, as DNS_MAX_RETRIES and DNS_TMR_INTERVAL have it. Its result is
 * one of lwIP's MEMP_NUM_NETDB, 1 by default, which the socket holds only while
 * it copies the address out, and waits for while another task's lookup holds
 * it, for half a second at the most, before failing with EAI_MEMORY. Connecting
 * takes the connect timeout at the most: see
 * ebisu_lwip_tcp_socket_set_connect_timeout(). It connects without blocking,
 * and waits for the connection with lwip_select(), or lwip_poll() where lwIP is
 * built without select.
 *
 * Its receive and send timeouts are SO_RCVTIMEO and SO_SNDTIMEO, which it sets
 * as lwIP takes them: an int of milliseconds in lwIP 1.4, and as
 * LWIP_SO_SNDRCVTIMEO_NONSTANDARD says from 2.0 on. A receive that times out is
 * EBISU_SOCK_FAIL, which recv_timed_out tells from a lost connection, and
 * leaves the connection as it was. A peer that closes the connection is the end
 * of the stream, and one that resets it a failure, as a send to a peer that has
 * gone is: but for lwIP 2.0's receive, which takes a reset for the end of the
 * stream too, and leaves a send after it to fail. recv_wait waits with
 * lwip_select() or lwip_poll().
 *
 * Why an operation failed, its last_error says: a host that didn't resolve,
 * with lwip_getaddrinfo()'s EAI_ error, an address that refused, a timeout, a
 * peer that has gone, or another failure, each with lwIP's errno. It also
 * goes to the logger set by ebisu_lwip_tcp_socket_set_logger(), if any.
 *
 * One serves one connection at a time, from one task at a time, and is used
 * again for the next once closed. Needs lwIP 1.4.1 or later, with an OS,
 * NO_SYS 0, and LWIP_SOCKET, LWIP_TCP, LWIP_DNS, LWIP_SO_RCVTIMEO and
 * LWIP_SO_SNDTIMEO. lwIP's sockets say why a call failed in errno, which has
 * to be each task's own where more than one task makes them, as newlib's is
 * with FreeRTOS's configUSE_NEWLIB_REENTRANT; or, in lwIP 1.4 built without
 * ERRNO, and in lwIP 2.0 built with LWIP_SOCKET_SET_ERRNO at 0, which then set
 * none, in each socket's own error, which the socket reads with SO_ERROR.
 * Initialize with ebisu_lwip_tcp_socket_init().
 */
typedef struct ebisu_lwip_tcp_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    int _socket; /**< \private The lwIP socket, or -1. */
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
    /** \private What ebisu_lwip_tcp_socket_set_keepalive() sets: the idle
     * time, 0 for no keepalive, the interval and the count. */
    unsigned int _keepalive_idle;
    unsigned int _keepalive_interval; /**< \private */
    unsigned int _keepalive_count; /**< \private */
    /** \private What ebisu_lwip_tcp_socket_set_nodelay() sets. */
    bool _nodelay;
#if defined(EBISU_TLS_RESUME)
    /** \private The system whose monotonic_ms times the address kept, set
     * by ebisu_lwip_tcp_socket_set_address_memory(), or NULL to keep none. */
    ebisu_system_t* _memory_system;
    /** \private The address the last connection was made to, a struct
     * sockaddr of at most 28 bytes, a struct sockaddr_in6's size, and its
     * size, 0 for none; the host it is of, by an FNV-1a hash of its name, and
     * the port; and when the lookup that gave it was, by _memory_system's
     * monotonic_ms. */
    unsigned char _last_address[28];
    unsigned char _last_address_size; /**< \private */
    uint32_t _last_host; /**< \private */
    uint16_t _last_port; /**< \private */
    uint64_t _last_looked_up_ms; /**< \private */
#endif
} ebisu_lwip_tcp_socket_t;

/**
 * \brief Clears the socket, with its timeouts, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] to_recv Receive timeout in seconds, 0 for none. For MQTT, the
 * one given to kii_set_mqtt_to_sock_recv().
 * \param [in] to_send Send timeout in seconds, 0 for none. It bounds
 * connecting too, unless ebisu_lwip_tcp_socket_set_connect_timeout() sets
 * another.
 */
void ebisu_lwip_tcp_socket_init(
        ebisu_lwip_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send);

/**
 * \brief Sets how long connecting to the host's address may take.
 *
 * Not the lookup, which lwIP's own settings bound.
 *
 * \param [out] sock The socket.
 * \param [in] seconds 0 for no bound, which waits for as long as lwIP tries,
 * as TCP_SYNMAXRTX has it.
 */
void ebisu_lwip_tcp_socket_set_connect_timeout(ebisu_lwip_tcp_socket_t* sock,
        unsigned int seconds);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Why an operation failed, at EBISU_LOG_ERROR: a host that didn't resolve or
 * connect, a send or a receive that failed. A receive that times out isn't
 * one. An option lwIP refuses, at EBISU_LOG_WARNING.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_lwip_tcp_socket_set_logger(ebisu_lwip_tcp_socket_t* sock,
        ebisu_logger_t* logger);

#if defined(EBISU_TLS_RESUME)
/**
 * \brief Has the socket connect again to the address its last connection
 * went to, in a build with EBISU_TLS_RESUME.
 *
 * lwIP's DNS client gives one of a host's addresses, and of a host that has
 * several, not always the same. A server whose addresses don't share their
 * TLS sessions resumes one only on the address that made it, so a TLS socket
 * over this one, such as the mbedTLS port's with
 * ebisu_mbedtls_socket_set_session_resume(), would resume few. Asked to, the
 * socket keeps the address of each connection it makes after a lookup, and
 * its next connection to the same host and port goes there, without looking
 * the host up, for an hour after that lookup, by system's monotonic_ms. Then
 * the host is looked up again, as it is when that address doesn't connect.
 *
 * Built as the TLS ports' resumption is: with EBISU_TLS_RESUME defined, by
 * CMake's -DEBISU_TLS_RESUME=ON, for the port and for everything using it, as
 * it changes the socket's layout. Built without it, as by default, this
 * function isn't there, and none of what it does is built.
 *
 * \param [in,out] sock The socket.
 * \param [in] system The system whose monotonic_ms times the address kept,
 * which has to outlive the socket, or NULL, as the socket starts, to keep
 * none, forgetting the one it has.
 */
void ebisu_lwip_tcp_socket_set_address_memory(ebisu_lwip_tcp_socket_t* sock,
        ebisu_system_t* system);
#endif

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
 * It applies from the next connect: SO_KEEPALIVE, and TCP_KEEPIDLE,
 * TCP_KEEPINTVL and TCP_KEEPCNT where lwIP is built with LWIP_TCP_KEEPALIVE.
 * Without it, lwIP probes after TCP_KEEPIDLE_DEFAULT, every
 * TCP_KEEPINTVL_DEFAULT, TCP_KEEPCNT_DEFAULT times, 2 hours, 75 seconds and 9
 * by default, and the socket says at EBISU_LOG_WARNING that it can't set
 * them. So does it of a value lwIP refuses, and connects without it.
 *
 * \param [out] sock The socket.
 * \param [in] idle_s Seconds idle before the first probe, or 0 for no
 * keepalive, as the socket starts.
 * \param [in] interval_s Seconds between probes, or 0 for lwIP's.
 * \param [in] count Probes unanswered before the connection is given up, or
 * 0 for lwIP's.
 */
void ebisu_lwip_tcp_socket_set_keepalive(ebisu_lwip_tcp_socket_t* sock,
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
void ebisu_lwip_tcp_socket_set_nodelay(ebisu_lwip_tcp_socket_t* sock, bool nodelay);

/**
 * \brief The name servers on lwIP, for platform's DNS probe.
 *
 * lwIP's DNS client does the lookups, which ebisu_lwip_tcp_socket_t asks it
 * for: this is only what the probe needs to ask whether its servers answer
 * first. A lookup can't be given a timeout: the client sends each server 4
 * queries, at once and then 1, 2 and 4 seconds in, as DNS_MAX_RETRIES and
 * DNS_TMR_INTERVAL have it, and gives up on it 3 seconds after the last, 7
 * seconds in all, before the next, and the task that looks up waits all the
 * while.
 *
 * The servers are those the client asks, in the order it asks them, on port
 * 53: the first of dns_getserver()'s, and each next while it is set, as DHCP,
 * DHCPv6, RA or dns_setserver() set them. Each lookup starts again from the
 * first. With none set, there are none. An IPv6 server has its zone as its
 * scope, the interface's index lwIP gives it, from lwIP 2.1 on.
 *
 * A server is asked over a UDP socket of lwIP's. lwIP tells such a socket
 * nothing of ICMP's port unreachable, so a server nothing listens on doesn't
 * answer in time, rather than refuse, and no server answering is EAI_FAIL, as
 * for lwip_getaddrinfo(), and as ebisu_lwip_tcp_socket_t's last_error has it.
 *
 * A name in lwIP's local host list, DNS_LOCAL_HOSTLIST, or in its cache of
 * answers, the client doesn't ask the servers for: the probe asks them all the
 * same.
 *
 * They have no state, so this serves everything, from any task but lwIP's
 * own, as reading the servers takes lwIP's core lock with
 * LWIP_TCPIP_CORE_LOCKING. Needs LWIP_DNS, LWIP_UDP and LWIP_SOCKET.
 *
 * \return The name servers.
 */
ebisu_dns_t* ebisu_lwip_dns(void);

#ifdef __cplusplus
}
#endif

#endif /* EBISU_LWIP_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
