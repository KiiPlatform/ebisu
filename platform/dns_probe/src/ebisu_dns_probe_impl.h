/* The parts of the probe the tests and the fuzzer call by themselves. */
#ifndef _EBISU_DNS_PROBE_IMPL
#define _EBISU_DNS_PROBE_IMPL

#include <ebisu_dns.h>
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

/* As many of the system's servers as are asked: as many as the C libraries
   take from /etc/resolv.conf, MAXNS. */
#define _EBISU_DNS_SERVERS_MAX 3

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

/* Asks each of count servers in turn, with dns's ask, but one that an earlier
   one repeats, until one answers, as ebisu_dns_probe() asks the system's. */
ebisu_dns_probe_result_t _ebisu_dns_probe_ask_servers(ebisu_dns_t* dns, const char* host,
        const ebisu_dns_addr_t* servers, size_t count, uint32_t timeout_ms, char* detail,
        size_t detail_size);

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_DNS_PROBE_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
