/* The part of the Win32 port's name servers the tests call by itself. */
#ifndef EBISU_WIN32_DNS_IMPL_H
#define EBISU_WIN32_DNS_IMPL_H

#include <ebisu_dns.h>

#include <winsock2.h>
#include <iphlpapi.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The servers of the adapters that are up, in the order they come, as many as
   max: IPv4 and IPv6, with an IPv6 one's scope, but a server an earlier one
   has and the placeholders fec0:0:0:ffff::1 to ::3, which Windows lists for an
   adapter without an IPv6 server. How many: it may be none. */
size_t _ebisu_win32_dns_adapter_servers(const IP_ADAPTER_ADDRESSES* adapters,
        ebisu_dns_addr_t* servers, size_t max);

#ifdef __cplusplus
}
#endif

#endif /* EBISU_WIN32_DNS_IMPL_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
