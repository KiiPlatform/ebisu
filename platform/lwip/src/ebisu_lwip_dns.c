/* The name servers on lwIP: those its DNS client asks, in the order it asks
   them, asked over a UDP socket of lwIP's. */

#include "ebisu_lwip.h"

#include <stdio.h>
#include <string.h>

#include <lwip/opt.h>
/* Before lwip/dns.h, which lwIP 2.0 has use err_t without it, and lwIP 1.4
   ip_addr_t too. */
#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/dns.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/tcpip.h>

#include "ebisu_lwip_impl.h"

#if NO_SYS
#error "ebisu_lwip_dns() asks over lwIP's sockets, which need an OS: set NO_SYS to 0."
#endif
#if !LWIP_SOCKET || !LWIP_UDP
#error "ebisu_lwip_dns() asks over lwIP's sockets: set LWIP_SOCKET and LWIP_UDP to 1."
#endif
#if !LWIP_DNS
#error "ebisu_lwip_dns() has the DNS client's servers: set LWIP_DNS to 1."
#endif

/* The port name servers listen on, as lwIP's DNS client asks them. */
#define DNS_PORT 53

/* A socket's address, of a family lwIP is built with: lwIP 1.4 has no struct
   sockaddr_storage. */
typedef union lwip_sockaddr_t {
    struct sockaddr sa;
#if EBISU_LWIP_IPV4
    struct sockaddr_in in;
#endif
#if EBISU_LWIP_IPV6
    struct sockaddr_in6 in6;
#endif
} lwip_sockaddr_t;

/* One of lwIP's servers as an ebisu_dns_addr_t. Whether it is one: not
   IP_ADDR_ANY, which the client takes for none set. */
static bool to_addr(const ip_addr_t* server, ebisu_dns_addr_t* out)
{
    if (server == NULL || ip_addr_isany(server)) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->port = DNS_PORT;
#if EBISU_LWIP_IPV6
    if (IP_IS_V6(server)) {
        out->family = EBISU_DNS_IPV6;
        /* Its words in network order, as lwIP keeps them. */
        memcpy(out->bytes, ip_2_ip6(server)->addr, 16);
#if LWIP_IPV6_SCOPES
        out->scope_id = ip6_addr_zone(ip_2_ip6(server));
#endif
        return true;
    }
#endif
#if EBISU_LWIP_IPV4
    out->family = EBISU_DNS_IPV4;
    /* In network order, as lwIP keeps it; lwIP 1.4's ip_addr_t is IPv4's. */
#if LWIP_VERSION_MAJOR < 2
    memcpy(out->bytes, &server->addr, 4);
#else
    memcpy(out->bytes, &ip_2_ip4(server)->addr, 4);
#endif
    return true;
#else
    return false;
#endif
}

/* The client's servers, as it asks them: from the first, and on to each next
   one while it is set. */
static size_t lwip_dns_servers(ebisu_dns_t* self, ebisu_dns_addr_t* out, size_t max)
{
    size_t count = 0;
    u8_t i;
    (void)self;
    /* DHCP and the application set them in lwIP's thread: not in the middle
       of this. */
#if LWIP_TCPIP_CORE_LOCKING
    LOCK_TCPIP_CORE();
#endif
    for (i = 0; i < DNS_MAX_SERVERS && count < max; ++i) {
#if LWIP_VERSION_MAJOR < 2
        /* A copy in lwIP 1.4. */
        ip_addr_t server = dns_getserver(i);
        if (!to_addr(&server, &out[count])) {
            break;
        }
#else
        if (!to_addr(dns_getserver(i), &out[count])) {
            break;
        }
#endif
        ++count;
    }
#if LWIP_TCPIP_CORE_LOCKING
    UNLOCK_TCPIP_CORE();
#endif
    return count;
}

/* The server as a socket's address, with its port. Its length, or 0 if lwIP
   is built without its family. */
static socklen_t to_sockaddr(const ebisu_dns_addr_t* server, lwip_sockaddr_t* out)
{
    memset(out, 0, sizeof(*out));
#if EBISU_LWIP_IPV6
    if (server->family == EBISU_DNS_IPV6) {
        struct sockaddr_in6* in6 = &out->in6;
        in6->sin6_len = (u8_t)sizeof(*in6);
        in6->sin6_family = AF_INET6;
        in6->sin6_port = lwip_htons(server->port);
        memcpy(&in6->sin6_addr, server->bytes, 16);
        in6->sin6_scope_id = server->scope_id;
        return (socklen_t)sizeof(*in6);
    }
#endif
#if EBISU_LWIP_IPV4
    if (server->family == EBISU_DNS_IPV4) {
        struct sockaddr_in* in = &out->in;
        in->sin_len = (u8_t)sizeof(*in);
        in->sin_family = AF_INET;
        in->sin_port = lwip_htons(server->port);
        memcpy(&in->sin_addr, server->bytes, 4);
        return (socklen_t)sizeof(*in);
    }
#endif
    return 0;
}

/* Whether a datagram came from the server: its family, address and port. */
static bool from_server(const lwip_sockaddr_t* from, const lwip_sockaddr_t* to)
{
#if EBISU_LWIP_IPV6
    if (to->sa.sa_family == AF_INET6) {
        const struct sockaddr_in6* f = &from->in6;
        const struct sockaddr_in6* t = &to->in6;
        return from->sa.sa_family == AF_INET6 && f->sin6_port == t->sin6_port &&
            memcmp(&f->sin6_addr, &t->sin6_addr, 16) == 0;
    }
#endif
#if EBISU_LWIP_IPV4
    if (to->sa.sa_family == AF_INET) {
        const struct sockaddr_in* f = &from->in;
        const struct sockaddr_in* t = &to->in;
        return from->sa.sa_family == AF_INET && f->sin_port == t->sin_port &&
            f->sin_addr.s_addr == t->sin_addr.s_addr;
    }
#endif
    return false;
}

static ebisu_dns_asked_t fail(const char* what, int err, char* why, size_t why_size)
{
    snprintf(why, why_size, "%s: errno=%d", what, err);
    return EBISU_DNS_ASKED_FAILED;
}

static ebisu_dns_asked_t lwip_dns_ask(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
        const unsigned char* query, size_t length, uint32_t timeout_ms,
        ebisu_dns_is_answer_t is_answer, void* userdata, char* why, size_t why_size)
{
    lwip_sockaddr_t to;
    ebisu_dns_asked_t asked = EBISU_DNS_ASKED_TIMEOUT;
    socklen_t to_length = to_sockaddr(server, &to);
    u32_t start;
    int s;
    (void)self;

    if (to_length == 0) {
        snprintf(why, why_size, "lwIP is built without IPv%d",
                server->family == EBISU_DNS_IPV6 ? 6 : 4);
        return EBISU_DNS_ASKED_FAILED;
    }
    s = lwip_socket(to.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        return fail("lwIP gave no socket", ebisu_lwip_error(-1), why, why_size);
    }
    /* From a port of its own, which lwIP picks. */
    if (lwip_sendto(s, query, length, 0, &to.sa, to_length) < 0) {
        int err = ebisu_lwip_error(s);
        (void)lwip_close(s);
        return fail("failed to send", err, why, why_size);
    }

    /* lwIP tells a UDP socket nothing of ICMP's port unreachable: a server
       nothing listens on doesn't answer. sys_now() is lwIP's clock, in
       milliseconds, which wraps: what has passed is told by subtracting. */
    start = sys_now();
    for (;;) {
        unsigned char datagram[512];
        lwip_sockaddr_t from;
        socklen_t from_length = sizeof(from);
        u32_t passed = sys_now() - start;
        long received;
        int ready;
        if (passed >= timeout_ms) {
            break;
        }
        ready = ebisu_lwip_wait(s, false, (long)(timeout_ms - passed));
        if (ready == 0) {
            break;
        }
        if (ready < 0) {
            asked = fail("failed to wait for the answer", ebisu_lwip_error(s), why, why_size);
            break;
        }
        memset(&from, 0, sizeof(from));
        received = (long)lwip_recvfrom(s, datagram, sizeof(datagram), 0,
                &from.sa, &from_length);
        if (received < 0) {
            asked = fail("failed to receive", ebisu_lwip_error(s), why, why_size);
            break;
        }
        if (from_server(&from, &to) && is_answer(datagram, (size_t)received, userdata)) {
            asked = EBISU_DNS_ASKED_ANSWERED;
            break;
        }
    }
    (void)lwip_close(s);
    return asked;
}

/* As lwip_getaddrinfo() fails a lookup no server answered, which the lwIP
   socket's last_error gives. */
static long lwip_dns_no_answer_error(ebisu_dns_t* self)
{
    (void)self;
    return EAI_FAIL;
}

static const ebisu_dns_ops_t lwip_dns_ops = {
    lwip_dns_servers,
    lwip_dns_ask,
    NULL,
    NULL,
    lwip_dns_no_answer_error
};

static ebisu_dns_t lwip_dns = { &lwip_dns_ops };

ebisu_dns_t* ebisu_lwip_dns(void)
{
    return &lwip_dns;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
