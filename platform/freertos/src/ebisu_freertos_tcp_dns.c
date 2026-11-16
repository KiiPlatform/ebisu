/* The name servers on FreeRTOS-Plus-TCP: those of its end-points, in the order
   its DNS client asks them, asked over a UDP socket of its own. */

#include "ebisu_freertos_tcp.h"

#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <FreeRTOS_DNS.h>
#include <FreeRTOS_IP.h>
#include <FreeRTOS_Routing.h>
#include <FreeRTOS_Sockets.h>

#if ipconfigUSE_DNS != 1
#error "ebisu_freertos_tcp_dns() has the DNS client's servers: set ipconfigUSE_DNS to 1."
#endif

/* The port name servers listen on. */
#define DNS_PORT 53

/* Ticks of milliseconds, rounded up, and a tick more: the tick under way
   counts as a whole one, so that a wait of n ticks can be a little less than
   n of their periods. As the kernel's port has its delay. */
static uint64_t wait_ticks(uint32_t msec)
{
    return ((uint64_t)msec * configTICK_RATE_HZ + 999u) / 1000u + 1u;
}

/* The servers of one end-point, as the DNS client goes through them: from the
   one its index is at, then each other that isn't 0, in turn, round to the
   first. One the client takes for no server at all, such as the broadcast
   address, ends them: its index stays there, and the client goes on to the
   next end-point that has one. Whether the client stays with this end-point
   for good, having gone round all of them. */
static bool add_servers(const unsigned char (*addresses)[16], size_t address_size,
        uint8_t index, bool (*usable)(const unsigned char*), int family,
        ebisu_dns_addr_t* out, size_t max, size_t* count)
{
    uint8_t i = index;
    if (index >= ipconfigENDPOINT_DNS_ADDRESS_COUNT) {
        return false;
    }
    for (;;) {
        if (!usable(addresses[i])) {
            return false;
        }
        if (*count < max) {
            memset(&out[*count], 0, sizeof(out[*count]));
            out[*count].family = family;
            memcpy(out[*count].bytes, addresses[i], address_size);
            out[*count].port = DNS_PORT;
            ++*count;
        }
        /* As prvIncreaseDNS4Index() and prvIncreaseDNS6Index() go on, to one
           whose first byte isn't 0 for IPv6. */
        do {
            i = (uint8_t)((i + 1u) % ipconfigENDPOINT_DNS_ADDRESS_COUNT);
        } while (i != index && addresses[i][0] == 0 && (address_size == 16 ||
                    (addresses[i][1] | addresses[i][2] | addresses[i][3]) == 0));
        if (i == index) {
            return true;
        }
    }
}

#if ipconfigUSE_IPv4 != 0
/* An IPv4 server the client asks, as prvFillSockAddress() has it: neither 0
   nor the broadcast address. */
static bool usable_ipv4(const unsigned char* address)
{
    static const unsigned char none[4] = { 0, 0, 0, 0 };
    static const unsigned char broadcast[4] = { 0xff, 0xff, 0xff, 0xff };
    return memcmp(address, none, 4) != 0 && memcmp(address, broadcast, 4) != 0;
}
#endif

#if ipconfigUSE_IPv6 != 0
/* An IPv6 server the client asks, as prvFillSockAddress() has it: neither of
   its first 2 bytes 0. */
static bool usable_ipv6(const unsigned char* address)
{
    return address[0] != 0 && address[1] != 0;
}
#endif

/* The end-points' servers, of the family the DNS client prefers, from those of
   the first end-point it takes, as it asks them. */
static size_t tcp_dns_servers(ebisu_dns_t* self, ebisu_dns_addr_t* out, size_t max)
{
    size_t count = 0;
    NetworkEndPoint_t* end_point;
    (void)self;
    if (max == 0) {
        return 0;
    }
    /* The IP task's DHCP and RA change them: not in the middle of this. */
    vTaskSuspendAll();
    for (end_point = FreeRTOS_FirstEndPoint(NULL); end_point != NULL;
            end_point = FreeRTOS_NextEndPoint(NULL, end_point)) {
        unsigned char addresses[ipconfigENDPOINT_DNS_ADDRESS_COUNT][16];
        size_t i;
#if ipconfigUSE_IPv4 != 0
        if (xDNS_IP_Preference == xPreferenceIPv4 && end_point->bits.bIPv6 == 0u) {
            /* Each in network order, as FreeRTOS-Plus-TCP keeps them. */
            for (i = 0; i < ipconfigENDPOINT_DNS_ADDRESS_COUNT; ++i) {
                memcpy(addresses[i], &end_point->ipv4_settings.ulDNSServerAddresses[i], 4);
            }
            if (add_servers((const unsigned char (*)[16])addresses, 4,
                        end_point->ipv4_settings.ucDNSIndex, usable_ipv4, EBISU_DNS_IPV4, out,
                        max, &count)) {
                break;
            }
        }
#endif
#if ipconfigUSE_IPv6 != 0
        if (xDNS_IP_Preference == xPreferenceIPv6 && end_point->bits.bIPv6 != 0u) {
            for (i = 0; i < ipconfigENDPOINT_DNS_ADDRESS_COUNT; ++i) {
                memcpy(addresses[i], end_point->ipv6_settings.xDNSServerAddresses[i].ucBytes,
                        16);
            }
            if (add_servers((const unsigned char (*)[16])addresses, 16,
                        end_point->ipv6_settings.ucDNSIndex, usable_ipv6, EBISU_DNS_IPV6, out,
                        max, &count)) {
                break;
            }
        }
#endif
    }
    (void)xTaskResumeAll();
    return count;
}

/* The server as FreeRTOS-Plus-TCP's address, with its port. Whether it has
   one of that family. */
static bool to_sockaddr(const ebisu_dns_addr_t* server, struct freertos_sockaddr* out)
{
    memset(out, 0, sizeof(*out));
    out->sin_len = (uint8_t)sizeof(*out);
    out->sin_port = FreeRTOS_htons(server->port);
#if ipconfigUSE_IPv4 != 0
    if (server->family == EBISU_DNS_IPV4) {
        out->sin_family = (uint8_t)FREERTOS_AF_INET4;
        memcpy(&out->sin_address.ulIP_IPv4, server->bytes, 4);
        return true;
    }
#endif
#if ipconfigUSE_IPv6 != 0
    if (server->family == EBISU_DNS_IPV6) {
        out->sin_family = (uint8_t)FREERTOS_AF_INET6;
        memcpy(out->sin_address.xIP_IPv6.ucBytes, server->bytes, 16);
        return true;
    }
#endif
    return false;
}

/* Whether a datagram came from the server. */
static bool from_server(const struct freertos_sockaddr* from,
        const struct freertos_sockaddr* server)
{
    if (from->sin_port != server->sin_port) {
        return false;
    }
#if ipconfigUSE_IPv6 != 0
    if (server->sin_family == (uint8_t)FREERTOS_AF_INET6) {
        return from->sin_family == (uint8_t)FREERTOS_AF_INET6 &&
            memcmp(from->sin_address.xIP_IPv6.ucBytes, server->sin_address.xIP_IPv6.ucBytes,
                    16) == 0;
    }
#endif
#if ipconfigUSE_IPv4 != 0
    return from->sin_family != (uint8_t)FREERTOS_AF_INET6 &&
        from->sin_address.ulIP_IPv4 == server->sin_address.ulIP_IPv4;
#else
    return false;
#endif
}

/* FreeRTOS-Plus-TCP's error, negated to a pdFREERTOS_ERRNO_* value, in why:
   as a number, which FreeRTOS_strerror_r() would name only where
   FreeRTOS-Plus-TCP is built with ipconfigHAS_PRINTF. */
static void say_error(BaseType_t error, char* why, size_t why_size)
{
    snprintf(why, why_size, "FreeRTOS-Plus-TCP's error %ld", (long)error);
}

static ebisu_dns_asked_t tcp_dns_ask(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
        const unsigned char* query, size_t length, uint32_t timeout_ms,
        ebisu_dns_is_answer_t is_answer, void* userdata, char* why, size_t why_size)
{
    struct freertos_sockaddr to;
    ebisu_dns_asked_t asked = EBISU_DNS_ASKED_TIMEOUT;
    uint64_t left = wait_ticks(timeout_ms);
    TickType_t last;
    Socket_t socket;
    BaseType_t ret;
    int32_t sent;
    (void)self;

    if (!to_sockaddr(server, &to)) {
        snprintf(why, why_size, "FreeRTOS-Plus-TCP is built without IPv%d",
                server->family == EBISU_DNS_IPV6 ? 6 : 4);
        return EBISU_DNS_ASKED_FAILED;
    }
    socket = FreeRTOS_socket(to.sin_family, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP);
    if (socket == FREERTOS_INVALID_SOCKET) {
        snprintf(why, why_size, "FreeRTOS-Plus-TCP gave no socket");
        return EBISU_DNS_ASKED_FAILED;
    }
    /* To a port of its own, which FreeRTOS-Plus-TCP picks. */
    ret = FreeRTOS_bind(socket, NULL, 0);
    if (ret != 0) {
        say_error(-ret, why, why_size);
        (void)FreeRTOS_closesocket(socket);
        return EBISU_DNS_ASKED_FAILED;
    }
    sent = FreeRTOS_sendto(socket, query, length, 0, &to, sizeof(to));
    if (sent <= 0) {
        if (sent == 0) {
            snprintf(why, why_size, "FreeRTOS-Plus-TCP sent nothing");
        } else {
            say_error((BaseType_t)-sent, why, why_size);
        }
        (void)FreeRTOS_closesocket(socket);
        return EBISU_DNS_ASKED_FAILED;
    }

    /* Counted in turns, each short of what a TickType_t holds, which may be
       16 bits. ICMP's port unreachable doesn't come to a UDP socket of
       FreeRTOS-Plus-TCP's: a server nothing listens on doesn't answer. */
    last = xTaskGetTickCount();
    while (left > 0) {
        unsigned char datagram[512];
        struct freertos_sockaddr from;
        socklen_t from_length = sizeof(from);
        TickType_t turn = left >= portMAX_DELAY ? (TickType_t)(portMAX_DELAY - 1) :
            (TickType_t)left;
        TickType_t now;
        TickType_t took;
        int32_t received;
        (void)FreeRTOS_setsockopt(socket, 0, FREERTOS_SO_RCVTIMEO, &turn, sizeof(turn));
        memset(&from, 0, sizeof(from));
        received = FreeRTOS_recvfrom(socket, datagram, sizeof(datagram), 0, &from,
                &from_length);
        now = xTaskGetTickCount();
        took = (TickType_t)(now - last);
        last = now;
        left = took >= left ? 0 : left - took;
        if (received > 0) {
            if (from_server(&from, &to) && is_answer(datagram, (size_t)received, userdata)) {
                asked = EBISU_DNS_ASKED_ANSWERED;
                break;
            }
        } else if (received < 0 && received != -pdFREERTOS_ERRNO_EWOULDBLOCK &&
                received != -pdFREERTOS_ERRNO_EINTR) {
            say_error((BaseType_t)-received, why, why_size);
            asked = EBISU_DNS_ASKED_FAILED;
            break;
        }
    }
    (void)FreeRTOS_closesocket(socket);
    return asked;
}

/* As FreeRTOS_getaddrinfo() fails a lookup no server answered, which the
   socket's last_error gives negated. */
static long tcp_dns_no_answer_error(ebisu_dns_t* self)
{
    (void)self;
    return pdFREERTOS_ERRNO_ENOENT;
}

static const ebisu_dns_ops_t tcp_dns_ops = {
    tcp_dns_servers,
    tcp_dns_ask,
    NULL,
    NULL,
    tcp_dns_no_answer_error
};

static ebisu_dns_t tcp_dns = { &tcp_dns_ops };

ebisu_dns_t* ebisu_freertos_tcp_dns(void)
{
    return &tcp_dns;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
