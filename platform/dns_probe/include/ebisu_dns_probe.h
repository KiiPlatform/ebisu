/**
 * @file ebisu_dns_probe.h
 * @brief Whether the system's name servers answer, asked before a lookup that
 * can't be bounded.
 */
#ifndef _EBISU_DNS_PROBE
#define _EBISU_DNS_PROBE

#include <ebisu_dns.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>
#include <ebisu_system.h>
#include <stdbool.h>
#include <stddef.h>
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
 * \brief What a probe found.
 */
typedef enum ebisu_dns_probe_result_t {
    /** \brief A name server answered, so a lookup now gets an answer, though
     * not necessarily an address. */
    EBISU_DNS_PROBE_ANSWERED,
    /** \brief The host needs no name server: an address, IPv4 in any form
     * inet_aton() takes or IPv6, localhost or a name under it, which RFC 6761
     * keeps out of DNS, or a name no query can carry, whose lookup fails at
     * once. */
    EBISU_DNS_PROBE_NOT_NEEDED,
    /** \brief No name server answered in time, or none could be asked: a
     * lookup now may wait for as long as the C library keeps asking again. */
    EBISU_DNS_PROBE_SILENT
} ebisu_dns_probe_result_t;

/**
 * \brief A name server to ask.
 */
typedef struct ebisu_dns_server_t {
    /** \brief Its address, as ebisu_dns_parse_address() reads it: IPv4, in
     * any form inet_aton() takes, or IPv6, with a scope after a '%' or
     * without, an interface's number or, for a link-local address, its name,
     * as in "fe80::1%eth0". */
    const char* address;
    uint16_t port; /**< \brief Its port, 53 for DNS. */
} ebisu_dns_server_t;

/**
 * \brief Room enough for any detail a probe gives, with its NUL.
 */
#define EBISU_DNS_PROBE_DETAIL_SIZE 128

/**
 * \brief Asks the system's name servers whether they answer.
 *
 * getaddrinfo() can't be given a timeout: when the name servers don't answer,
 * a lookup waits for as long as the C library keeps asking them again, which
 * can be minutes, and so does the task that looked up. This asks first, in a
 * time it bounds, so that the task can give up instead.
 *
 * It sends one recursive query, for the host's IPv4 addresses, over UDP, to
 * each of the servers the system's lookups ask in turn, until one answers.
 * Each is given timeout_ms. Any answer counts, that the name doesn't exist
 * too: nothing is looked up, only whether a server answers. A datagram that
 * isn't the answer to the query is skipped.
 *
 * The servers are the first 3 of those dns's servers gives, as the system's
 * lookups ask them: a server repeated takes a place, but is asked once. Each is
 * asked with dns's ask. platform's ports have the system's: ebisu_posix_dns(),
 * the servers of /etc/resolv.conf over BSD sockets, as the C libraries read
 * it, ebisu_win32_dns(), those of the network adapters over Winsock 2,
 * ebisu_freertos_tcp_dns(), those of FreeRTOS-Plus-TCP's end-points, as its
 * DNS client asks them, and ebisu_lwip_dns(), those lwIP's DNS client asks,
 * over lwIP's sockets.
 *
 * A name a lookup gets from elsewhere, such as /etc/hosts, still needs the
 * servers to answer: the probe tells only whether they do.
 *
 * \param [in] dns The system's name servers, and how one is asked.
 * \param [in] host The host to be looked up.
 * \param [in] timeout_ms How long each server has to answer, in milliseconds.
 * \param [out] detail For a log, what happened: which server answered, or what
 * the last one asked did, as in "127.0.0.1:53 didn't answer in time", or NULL.
 * It is cut short to fit, and is always terminated.
 * \param [in] detail_size Its size, EBISU_DNS_PROBE_DETAIL_SIZE for room for
 * any.
 * \return What it found.
 */
ebisu_dns_probe_result_t ebisu_dns_probe(
        ebisu_dns_t* dns,
        const char* host,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size);

/**
 * \brief Waits until one of the system's name servers answers.
 *
 * For a program started before its network is up, as at boot: rather than
 * fail its first lookup, it waits here, for a time it bounds.
 *
 * It probes as ebisu_dns_probe() does, and again every_ms after a probe that
 * found no server answering, until one isn't EBISU_DNS_PROBE_SILENT or max_ms
 * have gone by since it was called. "Answers" is ebisu_dns_probe()'s: any
 * answer, that the name doesn't exist too, and a host that needs no server.
 *
 * The time is the system's monotonic_ms, and the waits its delay_ms. A probe
 * is never followed by a wait that isn't followed by a probe: the last wait
 * is cut so that it ends max_ms after the call, and the last probe starts
 * then at the latest, taking what a probe takes on top, timeout_ms for each
 * server. With max_ms of 0, or a first probe that takes max_ms or longer,
 * there is one probe and no wait. With every_ms of 0 a probe follows the
 * last at once, the probes' own timeouts all there is between them.
 *
 * Without a system, or one without monotonic_ms or delay_ms, it can't wait:
 * it probes once, as with max_ms of 0.
 *
 * \param [in] dns The system's name servers, and how one is asked.
 * \param [in] system The system, for its monotonic_ms and delay_ms.
 * \param [in] host The host to be looked up.
 * \param [in] timeout_ms How long each server has to answer a probe, in
 * milliseconds.
 * \param [in] every_ms How long to wait after a probe no server answered
 * before the next, in milliseconds.
 * \param [in] max_ms How long to go on for at most, in milliseconds, from
 * the call to the start of the last probe.
 * \param [out] detail As for ebisu_dns_probe(), of the last probe, or NULL.
 * \param [in] detail_size Its size.
 * \return What the last probe found: EBISU_DNS_PROBE_SILENT if no server
 * answered in the time.
 */
ebisu_dns_probe_result_t ebisu_dns_probe_wait(
        ebisu_dns_t* dns,
        ebisu_system_t* system,
        const char* host,
        uint32_t timeout_ms,
        uint32_t every_ms,
        uint32_t max_ms,
        char* detail,
        size_t detail_size);

/**
 * \brief Asks the given name servers whether they answer.
 *
 * As ebisu_dns_probe() does the system's.
 *
 * \param [in] dns How a server is asked, and an interface's number for a
 * scope's name.
 * \param [in] host The host to be looked up.
 * \param [in] servers The servers, asked in turn, read as
 * ebisu_dns_parse_address() reads them, without keep_bad_scope. One whose
 * address isn't one, a scope too, is skipped.
 * \param [in] count How many there are. With none, the result is
 * EBISU_DNS_PROBE_SILENT for a host that needs one.
 * \param [in] timeout_ms How long each server has to answer, in milliseconds.
 * \param [out] detail As for ebisu_dns_probe(), or NULL.
 * \param [in] detail_size Its size.
 * \return What it found.
 */
ebisu_dns_probe_result_t ebisu_dns_probe_servers(
        ebisu_dns_t* dns,
        const char* host,
        const ebisu_dns_server_t* servers,
        size_t count,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size);

/**
 * \brief A socket that asks whether the name servers answer before another
 * connects.
 *
 * Its connect probes as ebisu_dns_probe() does, then has the other socket
 * connect, unless no server answered: then it fails, without the other socket
 * connecting, and its last_error is EBISU_SOCK_ERROR_DNS, with the resolver's
 * error for one that didn't answer, as dns's no_answer_error has it: EAI_AGAIN
 * for platform's POSIX and Win32 ports, which on Windows is WSATRY_AGAIN, and
 * pdFREERTOS_ERRNO_ENOENT for the FreeRTOS-Plus-TCP one's, as its socket's
 * last_error has a lookup that failed. A connect the other
 * socket goes on with, after EBISU_SOCK_AGAIN, isn't probed again. Close
 * closes the other socket, or does nothing if it didn't connect.
 *
 * Everything else is the other socket's: send, recv and last_error, and
 * recv_timed_out and recv_wait if it has them, and only then.
 *
 * Why a connect failed goes to the logger set by
 * ebisu_dns_probe_socket_set_logger(), if any: at EBISU_LOG_ERROR, that no
 * server answered, and at EBISU_LOG_INFO, once, that one answers again, after
 * how many connects failed.
 *
 * Initialize with ebisu_dns_probe_socket_init().
 */
typedef struct ebisu_dns_probe_socket_t {
    ebisu_socket_t base; /**< \brief What ebisu is given. */
    ebisu_socket_t* _inner; /**< \private The socket that connects. */
    ebisu_dns_t* _dns; /**< \private The name servers. */
    uint32_t _timeout_ms; /**< \private */
    /** \private The servers asked, or NULL for the system's. */
    const ebisu_dns_server_t* _servers;
    size_t _server_count; /**< \private */
    ebisu_logger_t* _logger; /**< \private */
    /** \private Whether _inner's connect returned EBISU_SOCK_AGAIN, and is to
     * be called again without probing. */
    bool _connecting;
    /** \private Whether _inner's connect was called since it was closed. */
    bool _inner_used;
    /** \private Whether the last connect failed because no server answered. */
    bool _silent;
    /** \private Connects in a row that failed because no server answered. */
    unsigned long _failures;
} ebisu_dns_probe_socket_t;

/**
 * \brief Initializes a socket that probes the system's name servers before
 * another connects, and makes base a socket of it.
 *
 * \param [out] sock The socket.
 * \param [in] inner The socket that connects, initialized, whose operations
 * sock takes: it has to outlive sock.
 * \param [in] dns The system's name servers, and how one is asked, as
 * ebisu_posix_dns(), ebisu_win32_dns(), ebisu_freertos_tcp_dns() or
 * ebisu_lwip_dns() gives them, which have to outlive sock.
 * \param [in] timeout_ms How long each server has to answer, in milliseconds.
 */
void ebisu_dns_probe_socket_init(
        ebisu_dns_probe_socket_t* sock,
        ebisu_socket_t* inner,
        ebisu_dns_t* dns,
        uint32_t timeout_ms);

/**
 * \brief Sets the name servers the socket asks.
 *
 * \param [out] sock The socket.
 * \param [in] servers The servers, as ebisu_dns_probe_servers() takes them,
 * which have to outlive the socket, or NULL, as the socket starts, for the
 * system's.
 * \param [in] count How many there are.
 */
void ebisu_dns_probe_socket_set_servers(
        ebisu_dns_probe_socket_t* sock,
        const ebisu_dns_server_t* servers,
        size_t count);

/**
 * \brief Sets where the socket's diagnostics go.
 *
 * Built with EBISU_NO_LOG, as -DEBISU_LOG=OFF builds it, the socket logs
 * nothing.
 *
 * \param [out] sock The socket.
 * \param [in] logger The logger, which has to outlive the socket, or NULL, as
 * the socket starts, for the diagnostics to go nowhere.
 */
void ebisu_dns_probe_socket_set_logger(ebisu_dns_probe_socket_t* sock, ebisu_logger_t* logger);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_DNS_PROBE */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
