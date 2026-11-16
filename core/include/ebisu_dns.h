/**
 * @file ebisu_dns.h
 * @brief The system's name servers, and asking one of them, for the DNS probe.
 */
#ifndef _EBISU_DNS
#define _EBISU_DNS

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

/** \brief An IPv4 address, for ebisu_dns_addr_t's family. */
#define EBISU_DNS_IPV4 4
/** \brief An IPv6 address, for ebisu_dns_addr_t's family. */
#define EBISU_DNS_IPV6 6

/** \brief A name server's address. */
typedef struct ebisu_dns_addr_t {
    /** \brief EBISU_DNS_IPV4 or EBISU_DNS_IPV6. */
    int family;
    /** \brief The address, in network order: the first 4 bytes of it for
     * IPv4. */
    unsigned char bytes[16];
    /** \brief Its port, 53 for DNS. */
    uint16_t port;
    /** \brief For IPv6, the scope, the interface's number as the system has
     * it, or 0 for none. */
    uint32_t scope_id;
} ebisu_dns_addr_t;

/** \brief What a name server did with a query. */
typedef enum ebisu_dns_asked_t {
    /** \brief A datagram came that is_answer took. */
    EBISU_DNS_ASKED_ANSWERED,
    /** \brief None came in the time given. */
    EBISU_DNS_ASKED_TIMEOUT,
    /** \brief The system was told nothing listens there, as ICMP's port
     * unreachable says. */
    EBISU_DNS_ASKED_REFUSED,
    /** \brief The query couldn't be sent, or the answer received: no
     * socket, no route. */
    EBISU_DNS_ASKED_FAILED
} ebisu_dns_asked_t;

/** \brief Whether a datagram is the answer to the query, for
 * ebisu_dns_ops_t::ask.
 *
 * \param [in] datagram The datagram.
 * \param [in] length Its length.
 * \param [in] userdata What ask was given with it.
 */
typedef bool (*ebisu_dns_is_answer_t)(const unsigned char* datagram, size_t length,
        void* userdata);

/** \brief The system's name servers. See struct ebisu_dns_t. */
typedef struct ebisu_dns_t ebisu_dns_t;

/** \brief What the system's name servers are, and how one is asked.
 *
 * Ports implement these for their kind of system, as platform's POSIX, Win32
 * and FreeRTOS-Plus-TCP ports do with ebisu_posix_dns(), ebisu_win32_dns() and
 * ebisu_freertos_tcp_dns(), and an application for a system of its own. See ebisu_object.h for how self leads
 * to its state, if it has any.
 */
typedef struct ebisu_dns_ops_t {
    /** \brief The name servers the system's lookups ask, in the order they
     * ask them.
     *
     * \param [in] self The name servers.
     * \param [out] out Where they go.
     * \param [in] max How many out has room for.
     * \return How many there are, at most max: it may be none.
     */
    size_t (*servers)(ebisu_dns_t* self, ebisu_dns_addr_t* out, size_t max);

    /** \brief Sends a query to a server over UDP, and waits for its answer.
     *
     * Each datagram that comes from the server before timeout_ms runs out is
     * given to is_answer, and one it takes ends the wait. Other datagrams are
     * skipped. It must not wait longer than timeout_ms, nor end sooner but
     * with the answer or a failure.
     *
     * \param [in] self The name servers.
     * \param [in] server The server.
     * \param [in] query The query.
     * \param [in] length Its length.
     * \param [in] timeout_ms How long the server has to answer, in
     * milliseconds.
     * \param [in] is_answer Whether a datagram is the answer.
     * \param [in] userdata For is_answer.
     * \param [out] why For EBISU_DNS_ASKED_FAILED, what failed, as the system
     * says it, such as strerror()'s text. Terminated, cut short to fit.
     * \param [in] why_size Its size, at least 1.
     * \return What the server did.
     */
    ebisu_dns_asked_t (*ask)(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
            const unsigned char* query, size_t length, uint32_t timeout_ms,
            ebisu_dns_is_answer_t is_answer, void* userdata, char* why, size_t why_size);

    /** \brief The number of the interface of this name, for an IPv6 scope
     * given by name, as in "fe80::1%eth0".
     *
     * Optional: without it, a scope is a number only.
     *
     * \param [in] self The name servers.
     * \param [in] name The interface's name.
     * \return Its number, or 0 if there is none of that name.
     */
    uint32_t (*scope_of)(ebisu_dns_t* self, const char* name);

    /** \brief The name of the interface of this number, for a scope as it is
     * said.
     *
     * Optional: without it, a scope is said as its number.
     *
     * \param [in] self The name servers.
     * \param [in] scope_id The interface's number.
     * \param [out] out Its name, terminated.
     * \param [in] size out's size.
     * \return Whether there is one of that number, whose name fits.
     */
    bool (*scope_name)(ebisu_dns_t* self, uint32_t scope_id, char* out, size_t size);

    /** \brief The resolver's error for a lookup no server answered, as
     * getaddrinfo()'s EAI_AGAIN.
     *
     * Optional: without it, it is 0. The probing socket of ebisu_dns_probe.h
     * gives it as its last_error's detail.
     *
     * \param [in] self The name servers.
     * \return The error.
     */
    long (*no_answer_error)(ebisu_dns_t* self);
} ebisu_dns_ops_t;

/** \brief The system's name servers, and how one is asked: what the DNS probe
 * of platform's ebisu_dns_probe.h needs of the system.
 *
 * Embed it in the struct holding the state, as ebisu_object.h shows.
 */
struct ebisu_dns_t {
    const ebisu_dns_ops_t* ops; /**< \brief What it does. */
};

/** \brief Room enough for any address ebisu_dns_format_address() writes, with
 * its NUL, but for a scope's name. */
#define EBISU_DNS_ADDRESS_TEXT_SIZE (1 + 45 + 1 + 10 + 2 + 5 + 1)

/**
 * \brief Reads an address of a name server.
 *
 * IPv4 in any form inet_aton() takes, numbers of 1 to 4 parts, each decimal,
 * octal after a 0 or hexadecimal after 0x, or IPv6, as inet_pton() takes it,
 * with a scope after a '%' or without. The scope is an interface's number, in
 * decimal digits only, or, for a link-local address, unicast or multicast, its
 * name, as dns's scope_of gives its number. A scope that is neither is refused,
 * as musl does, or, with keep_bad_scope, left out, as glibc does.
 *
 * \param [in] dns For a scope's name, or NULL, for which there is none.
 * \param [in] s The text, of length bytes, without anything after the address.
 * \param [in] length Its length.
 * \param [in] port The port out has.
 * \param [in] keep_bad_scope Whether an address whose scope isn't one is taken
 * without it.
 * \param [out] out The address.
 * \return Whether it is one.
 */
bool ebisu_dns_parse_address(ebisu_dns_t* dns, const char* s, size_t length, uint16_t port,
        bool keep_bad_scope, ebisu_dns_addr_t* out);

/**
 * \brief Writes a name server's address, for a log: "address:port", or
 * "[address]:port" or "[address%scope]:port" for IPv6.
 *
 * IPv4 in dotted decimal, IPv6 as RFC 5952 says: in lowercase hexadecimal,
 * without leading zeros, the longest run of 2 groups of 0 or more, the first
 * of the longest, as "::", and an IPv4-mapped address's last 32 bits in dotted
 * decimal. The scope is the interface's name, as dns's scope_name gives it, or
 * its number.
 *
 * \param [in] dns For the scope's name, or NULL, for its number.
 * \param [in] addr The address.
 * \param [out] out The text, terminated, cut short to fit.
 * \param [in] size out's size, EBISU_DNS_ADDRESS_TEXT_SIZE for any without a
 * scope's name.
 */
void ebisu_dns_format_address(ebisu_dns_t* dns, const ebisu_dns_addr_t* addr, char* out,
        size_t size);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_DNS */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
