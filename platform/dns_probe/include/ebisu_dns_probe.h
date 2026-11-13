/**
 * @file ebisu_dns_probe.h
 * @brief Whether the system's name servers answer, asked before a lookup that
 * can't be bounded.
 */
#ifndef _EBISU_DNS_PROBE
#define _EBISU_DNS_PROBE

#include <ebisu_log.h>
#include <ebisu_socket.h>
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
    /** \brief Its address: IPv4, in any form inet_aton() takes, or IPv6,
     * with a scope after a '%' or without, an interface's number or, for a
     * link-local address, its name, as in "fe80::1%eth0". */
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
 * The servers are those of the first 3 nameserver lines of /etc/resolv.conf
 * whose address is one, in order, on port 53, as the C libraries take them. A
 * server a line repeats takes a place, but is asked once. An IPv6 address's
 * scope is read as ebisu_dns_server_t's is. A line with another scope is asked
 * without it where the probe is built with glibc, and skipped where it isn't,
 * as glibc and musl do. So are the lines read: with glibc, split at spaces and
 * tabs, and whole, however long, and otherwise split at what isspace() takes,
 * a line longer than 254 bytes skipped. An address ends at a blank: a '#' or a
 * ';' right after it makes it one no longer. Without a server, or without the
 * file, it is 127.0.0.1, as for the C libraries.
 *
 * A name a lookup gets from elsewhere, such as /etc/hosts, still needs the
 * servers to answer: the probe tells only whether they do.
 *
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
        const char* host,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size);

/**
 * \brief Asks the given name servers whether they answer.
 *
 * As ebisu_dns_probe() does the system's.
 *
 * \param [in] host The host to be looked up.
 * \param [in] servers The servers, asked in turn. One whose address isn't one,
 * a scope too, is skipped.
 * \param [in] count How many there are. With none, the result is
 * EBISU_DNS_PROBE_SILENT for a host that needs one.
 * \param [in] timeout_ms How long each server has to answer, in milliseconds.
 * \param [out] detail As for ebisu_dns_probe(), or NULL.
 * \param [in] detail_size Its size.
 * \return What it found.
 */
ebisu_dns_probe_result_t ebisu_dns_probe_servers(
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
 * connecting, and its last_error is EBISU_SOCK_ERROR_DNS, with EAI_AGAIN,
 * getaddrinfo()'s error for a resolver that didn't answer. A connect the other
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
 * \param [in] timeout_ms How long each server has to answer, in milliseconds.
 */
void ebisu_dns_probe_socket_init(
        ebisu_dns_probe_socket_t* sock,
        ebisu_socket_t* inner,
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
