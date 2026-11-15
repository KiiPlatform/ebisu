/* The parts of the POSIX port's name servers the tests and the fuzzer call by
   themselves. */
#ifndef _EBISU_POSIX_DNS_IMPL
#define _EBISU_POSIX_DNS_IMPL

#include <ebisu_dns.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the C library reads /etc/resolv.conf as glibc does, or as musl does,
   which the others are taken to. */
#ifdef __GLIBC__
#define _EBISU_POSIX_DNS_READ_AS_GLIBC true
#else
#define _EBISU_POSIX_DNS_READ_AS_GLIBC false
#endif

/* Adds the server of a line of /etc/resolv.conf, of length bytes without its
   newline, to the count in servers, unless it isn't a nameserver line, its
   address isn't one, or there are max. The new count. The line is read as
   glibc reads it, with as_glibc, or as musl does: split at a space or a tab,
   or at what isspace() takes, its address up to the first, and taken as
   ebisu_dns_parse_address() takes it, with keep_bad_scope as_glibc. A server
   repeated takes a place again, as in the C libraries. */
size_t _ebisu_posix_dns_parse_resolv_line(const char* line, size_t length,
        ebisu_dns_addr_t* servers, size_t count, size_t max, bool as_glibc);

/* The servers of a whole /etc/resolv.conf, of length bytes, as many as max.
   How many: it may be none. */
size_t _ebisu_posix_dns_parse_resolv_conf(const char* text, size_t length,
        ebisu_dns_addr_t* servers, size_t max, bool as_glibc);

/* The servers of the file at path, as the system's lookups ask them, as many as
   max, which is at least 1: 127.0.0.1 if it has none or can't be read. How
   many. Its lines are read as _ebisu_posix_dns_parse_resolv_line() has it,
   each whole, with as_glibc, or, as musl does, one longer than 254 bytes, its
   newline aside, skipped. */
size_t _ebisu_posix_dns_read_servers(const char* path, ebisu_dns_addr_t* servers,
        size_t max, bool as_glibc);

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_POSIX_DNS_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
