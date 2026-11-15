/* platform's Win32 port's name servers: those of the network adapters, asked
   over Winsock 2. */

#include "ebisu_win32.h"
#include "ebisu_win32_dns_impl.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The port name servers listen on. */
#define DNS_PORT 53

static bool same_server(const ebisu_dns_addr_t* a, const ebisu_dns_addr_t* b)
{
    return a->family == b->family && a->port == b->port && a->scope_id == b->scope_id &&
        memcmp(a->bytes, b->bytes, sizeof(a->bytes)) == 0;
}

/* The placeholders fec0:0:0:ffff::1, ::2 and ::3, site-local, which Windows
   lists as the name servers of an adapter without an IPv6 one of its own, and
   which nothing answers on. */
static bool is_placeholder(const unsigned char bytes[16])
{
    static const unsigned char prefix[15] = {
        0xfe, 0xc0, 0, 0, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0
    };
    return memcmp(bytes, prefix, sizeof(prefix)) == 0 && bytes[15] >= 1 && bytes[15] <= 3;
}

size_t _ebisu_win32_dns_adapter_servers(const IP_ADAPTER_ADDRESSES* adapters,
        ebisu_dns_addr_t* servers, size_t max)
{
    size_t count = 0;
    const IP_ADAPTER_ADDRESSES* adapter;
    for (adapter = adapters; adapter != NULL && count < max; adapter = adapter->Next) {
        const IP_ADAPTER_DNS_SERVER_ADDRESS* dns;
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        for (dns = adapter->FirstDnsServerAddress; dns != NULL && count < max;
                dns = dns->Next) {
            const SOCKADDR* sa = dns->Address.lpSockaddr;
            ebisu_dns_addr_t server;
            size_t i;
            if (sa == NULL) {
                continue;
            }
            memset(&server, 0, sizeof(server));
            server.port = DNS_PORT;
            if (sa->sa_family == AF_INET &&
                    dns->Address.iSockaddrLength >= (INT)sizeof(SOCKADDR_IN)) {
                server.family = EBISU_DNS_IPV4;
                memcpy(server.bytes, &((const SOCKADDR_IN*)sa)->sin_addr, 4);
            } else if (sa->sa_family == AF_INET6 &&
                    dns->Address.iSockaddrLength >= (INT)sizeof(SOCKADDR_IN6)) {
                const SOCKADDR_IN6* in6 = (const SOCKADDR_IN6*)sa;
                server.family = EBISU_DNS_IPV6;
                memcpy(server.bytes, &in6->sin6_addr, 16);
                server.scope_id = in6->sin6_scope_id;
                if (is_placeholder(server.bytes)) {
                    continue;
                }
            } else {
                continue;
            }
            /* Adapters often share their servers: each takes one place. */
            for (i = 0; i < count && !same_server(&servers[i], &server); ++i) {
            }
            if (i == count) {
                servers[count++] = server;
            }
        }
    }
    return count;
}

/* The servers the system's lookups ask, as many as max, from the adapters
   GetAdaptersAddresses() gives. */
static size_t win32_servers(ebisu_dns_t* self, ebisu_dns_addr_t* servers, size_t max)
{
    const ULONG flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME;
    ULONG size = 16 * 1024;
    int tries;
    (void)self;
    /* Adapters that come meanwhile can make the size it asked for too small
       again. */
    for (tries = 0; tries < 3; ++tries) {
        IP_ADAPTER_ADDRESSES* adapters = (IP_ADAPTER_ADDRESSES*)malloc(size);
        ULONG ret;
        if (adapters == NULL) {
            return 0;
        }
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &size);
        if (ret == NO_ERROR) {
            size_t count = _ebisu_win32_dns_adapter_servers(adapters, servers, max);
            free(adapters);
            return count;
        }
        free(adapters);
        if (ret != ERROR_BUFFER_OVERFLOW) {
            return 0;
        }
    }
    return 0;
}

/* In microseconds, for a wait never to end before the time given, as one
   counted from a time cut to the millisecond could. */
static int64_t monotonic_us(void)
{
    LARGE_INTEGER frequency;
    LARGE_INTEGER now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (int64_t)(now.QuadPart / frequency.QuadPart * 1000000 +
            now.QuadPart % frequency.QuadPart * 1000000 / frequency.QuadPart);
}

/* ICMP's port unreachable, which Winsock reports on a UDP socket as a reset. */
static bool is_refused(int error)
{
    return error == WSAECONNRESET || error == WSAECONNREFUSED;
}

/* Sends query to server, and waits timeout_ms for a datagram is_answer takes.
   What it did, with the Winsock error in out_errno for
   EBISU_DNS_ASKED_FAILED. */
static ebisu_dns_asked_t ask_once(const ebisu_dns_addr_t* server, const unsigned char* query,
        size_t query_length, uint32_t timeout_ms, ebisu_dns_is_answer_t is_answer,
        void* userdata, int* out_errno)
{
    struct sockaddr_storage addr;
    int addr_length;
    int64_t deadline_us;
    ebisu_dns_asked_t asked;
    u_long nonblocking = 1;
    SOCKET sock;

    memset(&addr, 0, sizeof(addr));
    if (server->family == EBISU_DNS_IPV4) {
        struct sockaddr_in* in = (struct sockaddr_in*)&addr;
        in->sin_family = AF_INET;
        in->sin_port = htons(server->port);
        memcpy(&in->sin_addr, server->bytes, 4);
        addr_length = (int)sizeof(*in);
    } else {
        struct sockaddr_in6* in6 = (struct sockaddr_in6*)&addr;
        in6->sin6_family = AF_INET6;
        in6->sin6_port = htons(server->port);
        memcpy(&in6->sin6_addr, server->bytes, 16);
        in6->sin6_scope_id = server->scope_id;
        addr_length = (int)sizeof(*in6);
    }

    *out_errno = 0;
    sock = socket(server->family == EBISU_DNS_IPV4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        *out_errno = WSAGetLastError();
        return EBISU_DNS_ASKED_FAILED;
    }
    /* Connected, so that only the server's datagrams are received, and its
       port unreachable is told. Not waiting but in select(). */
    if (ioctlsocket(sock, FIONBIO, &nonblocking) != 0 ||
            connect(sock, (struct sockaddr*)&addr, addr_length) != 0 ||
            send(sock, (const char*)query, (int)query_length, 0) == SOCKET_ERROR) {
        *out_errno = WSAGetLastError();
        closesocket(sock);
        return is_refused(*out_errno) ? EBISU_DNS_ASKED_REFUSED : EBISU_DNS_ASKED_FAILED;
    }

    deadline_us = monotonic_us() + (int64_t)timeout_ms * 1000;
    for (;;) {
        unsigned char datagram[512];
        fd_set readable;
        struct timeval wait;
        /* In milliseconds, rounded up. */
        int64_t left = (deadline_us - monotonic_us() + 999) / 1000;
        int received;
        int ready;
        if (left <= 0) {
            asked = EBISU_DNS_ASKED_TIMEOUT;
            break;
        }
        if (left > 60000) {
            left = 60000;
        }
        FD_ZERO(&readable);
        FD_SET(sock, &readable);
        wait.tv_sec = (long)(left / 1000);
        wait.tv_usec = (long)(left % 1000) * 1000;
        /* Winsock ignores the first argument. */
        ready = select(0, &readable, NULL, NULL, &wait);
        if (ready == 0) {
            continue;
        }
        if (ready == SOCKET_ERROR) {
            *out_errno = WSAGetLastError();
            asked = EBISU_DNS_ASKED_FAILED;
            break;
        }
        received = recv(sock, (char*)datagram, (int)sizeof(datagram), 0);
        if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                continue;
            }
            if (error == WSAEMSGSIZE) {
                /* Longer than the buffer, which holds its start: the header
                   is enough to tell an answer. */
                received = (int)sizeof(datagram);
            } else {
                *out_errno = error;
                asked = is_refused(error) ? EBISU_DNS_ASKED_REFUSED : EBISU_DNS_ASKED_FAILED;
                break;
            }
        }
        if (is_answer(datagram, (size_t)received, userdata)) {
            asked = EBISU_DNS_ASKED_ANSWERED;
            break;
        }
    }
    closesocket(sock);
    return asked;
}

static ebisu_dns_asked_t win32_ask(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
        const unsigned char* query, size_t length, uint32_t timeout_ms,
        ebisu_dns_is_answer_t is_answer, void* userdata, char* why, size_t why_size)
{
    int error = 0;
    ebisu_dns_asked_t asked = ask_once(server, query, length, timeout_ms, is_answer, userdata,
            &error);
    (void)self;
    if (asked == EBISU_DNS_ASKED_FAILED && why_size > 0) {
        DWORD text = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, (DWORD)error, 0, why, (DWORD)why_size, NULL);
        while (text > 0 && (why[text - 1] == '\r' || why[text - 1] == '\n' ||
                    why[text - 1] == ' ' || why[text - 1] == '.')) {
            --text;
        }
        if (text == 0) {
            snprintf(why, why_size, "error %d", error);
        } else {
            why[text] = '\0';
        }
    }
    return asked;
}

static uint32_t win32_scope_of(ebisu_dns_t* self, const char* name)
{
    (void)self;
    return (uint32_t)if_nametoindex(name);
}

static bool win32_scope_name(ebisu_dns_t* self, uint32_t scope_id, char* out, size_t size)
{
    /* As if_indextoname() asks: mingw-w64's IF_NAMESIZE names
       NDIS_IF_MAX_STRING_SIZE, which its headers leave undefined here. */
    char name[IF_MAX_STRING_SIZE + 1];
    (void)self;
    if (if_indextoname(scope_id, name) == NULL || strlen(name) >= size) {
        return false;
    }
    memcpy(out, name, strlen(name) + 1);
    return true;
}

static long win32_no_answer_error(ebisu_dns_t* self)
{
    (void)self;
    /* getaddrinfo()'s EAI_AGAIN, which ws2tcpip.h defines as this. */
    return WSATRY_AGAIN;
}

static const ebisu_dns_ops_t win32_dns_ops = {
    win32_servers,
    win32_ask,
    win32_scope_of,
    win32_scope_name,
    win32_no_answer_error
};

static ebisu_dns_t win32_dns = { &win32_dns_ops };

ebisu_dns_t* ebisu_win32_dns(void)
{
    return &win32_dns;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
