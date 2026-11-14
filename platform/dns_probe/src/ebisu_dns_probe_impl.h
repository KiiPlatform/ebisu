/* The parts of the probe the tests and the fuzzer call by themselves. */
#ifndef _EBISU_DNS_PROBE_IMPL
#define _EBISU_DNS_PROBE_IMPL

#include <ebisu_dns_probe.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest name a query carries, without a trailing dot: 255 bytes in a
   query, a length before each label and the root's 0 at the end. */
#define _EBISU_DNS_NAME_MAX 253

/* The longest label. */
#define _EBISU_DNS_LABEL_MAX 63

/* The longest query built: the header, the name and its type and class. */
#define _EBISU_DNS_QUERY_MAX (12 + _EBISU_DNS_NAME_MAX + 2 + 4)

/* As many servers as the C libraries take from /etc/resolv.conf, MAXNS. */
#define _EBISU_DNS_SERVERS_MAX 3

/* Whether the C library reads /etc/resolv.conf as glibc does, or as musl does,
   which the others are taken to. */
#ifdef __GLIBC__
#define _EBISU_DNS_READ_AS_GLIBC true
#else
#define _EBISU_DNS_READ_AS_GLIBC false
#endif

/* An address of a name server. */
typedef struct _ebisu_dns_addr_t {
    int family; /* AF_INET or AF_INET6. */
    unsigned char bytes[16]; /* 4 of them for IPv4. */
    uint16_t port;
    uint32_t scope_id; /* The interface of an IPv6 address, or 0. */
} _ebisu_dns_addr_t;

/* A recursive query for host's IPv4 addresses, with id, in out. Its length, or
   0 if out is too small or no query can carry host: an empty name or label, a
   label longer than _EBISU_DNS_LABEL_MAX or a name longer than
   _EBISU_DNS_NAME_MAX, a trailing dot aside. */
size_t _ebisu_dns_probe_build_query(const char* host, uint16_t id, unsigned char* out,
        size_t size);

/* Whether the datagram of length bytes is an answer to the query with id. */
bool _ebisu_dns_probe_is_answer(const unsigned char* datagram, size_t length, uint16_t id);

/* Whether host needs a name server to be looked up. */
bool _ebisu_dns_probe_needs_lookup(const char* host);

/* s, of length bytes, as an IPv4 address in a form inet_aton() takes, without
   anything after it, in out, in network order. Whether it is one. */
bool _ebisu_dns_probe_parse_ipv4(const char* s, size_t length, unsigned char out[4]);

/* s, of length bytes, as an IPv4 address, as _ebisu_dns_probe_parse_ipv4()
   takes, or IPv6, with a scope after a '%' or without, in out with port.
   Whether it is one. The scope is an interface's number, in decimal, or, for a
   link-local address, its name. One that is neither is refused, as musl does,
   or, with keep_bad_scope, left out, as glibc does. */
bool _ebisu_dns_probe_parse_address(const char* s, size_t length, uint16_t port,
        bool keep_bad_scope, _ebisu_dns_addr_t* out);

/* Adds the server of a line of /etc/resolv.conf, of length bytes without its
   newline, to the count in servers, unless it isn't a nameserver line, its
   address isn't one, or there are max. The new count. The line is read as
   glibc reads it, with as_glibc, or as musl does: split at a space or a tab,
   or at what isspace() takes, its address up to the first, and taken as
   _ebisu_dns_probe_parse_address() takes it, with keep_bad_scope as_glibc. A
   server repeated takes a place again, as in the C libraries. */
size_t _ebisu_dns_probe_parse_resolv_line(const char* line, size_t length,
        _ebisu_dns_addr_t* servers, size_t count, size_t max, bool as_glibc);

/* The servers of a whole /etc/resolv.conf, of length bytes, as many as max.
   How many: it may be none. */
size_t _ebisu_dns_probe_parse_resolv_conf(const char* text, size_t length,
        _ebisu_dns_addr_t* servers, size_t max, bool as_glibc);

/* The servers of the file at path, as the system's lookups ask them, as many as
   max, which is at least 1: 127.0.0.1 if it has none or can't be read. How
   many. Its lines are read as _ebisu_dns_probe_parse_resolv_line() has it,
   each whole, with as_glibc, or, as musl does, one longer than 254 bytes, its
   newline aside, skipped. */
size_t _ebisu_dns_probe_read_servers(const char* path, _ebisu_dns_addr_t* servers,
        size_t max, bool as_glibc);

/* Asks each of count servers in turn, but one that an earlier one repeats,
   until one answers, as ebisu_dns_probe() asks the system's. */
ebisu_dns_probe_result_t _ebisu_dns_probe_ask_servers(const char* host,
        const _ebisu_dns_addr_t* servers, size_t count, uint32_t timeout_ms, char* detail,
        size_t detail_size);

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_DNS_PROBE_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
