// What the lwIP port's tests share: lwIP's headers, a logger that keeps its
// lines, and servers of the test's own on lwIP's loopback interface. See
// README.md.
#ifndef EBISU_LWIP_TEST_H
#define EBISU_LWIP_TEST_H

#include "catch.hpp"

#include <ebisu_lwip.h>

#include "lwip/opt.h"
/* Before lwip/dns.h, which lwIP 2.0 has use err_t without it, and lwIP 1.4
   ip_addr_t too. */
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "lwip/init.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"
#if LWIP_VERSION_MAJOR < 2
/* For TCP_KEEPINTVL_DEFAULT and TCP_KEEPCNT_DEFAULT, and the connections'
   pcbs. lwIP 1.4 has errno's names in lwip/arch.h, which lwip/sockets.h
   includes. */
#include "lwip/tcp_impl.h"
#else
#include "lwip/errno.h"
/* For TCP_KEEPINTVL_DEFAULT and TCP_KEEPCNT_DEFAULT. */
#include "lwip/priv/tcp_priv.h"
#endif

#if LWIP_VERSION_MAJOR < 2
/* lwIP 1.4 has IPv4 alone, whose addresses are ip_addr_t: by 2.0's names. */
typedef ip_addr_t ip4_addr_t;
inline int ip4addr_aton(const char* text, ip4_addr_t* addr)
{
    return ipaddr_aton(text, addr);
}
#endif

/* Whether SO_RCVTIMEO and SO_SNDTIMEO take an int of milliseconds, as lwIP
   1.4's always do, rather than a struct timeval. */
#if LWIP_VERSION_MAJOR < 2 || LWIP_SO_SNDRCVTIMEO_NONSTANDARD
#define TEST_TIMEOUT_MS 1
#else
#define TEST_TIMEOUT_MS 0
#endif

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>


// The address on the interface that drops all it is given, 10.255.0.1/24, and
// another: nothing there ever answers.
const char* const SILENT = "10.255.0.2";
const char* const SILENT_TOO = "10.255.0.3";

namespace {

// A logger that keeps each line it is given, with its level.
struct Logs {
    struct Logger {
        ebisu_logger_t base;
        Logs* owner;
    };
    std::vector<std::pair<ebisu_log_level_t, std::string>> lines;
    ebisu_logger_ops_t ops;
    Logger logger;

    static void log(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt,
            va_list args) {
        char line[512];
        vsnprintf(line, sizeof(line), fmt, args);
        reinterpret_cast<Logger*>(self)->owner->lines.emplace_back(level, line);
    }

    Logs() {
        ops.log = log;
        logger.base.ops = &ops;
        logger.owner = this;
    }
    Logs(const Logs&) = delete;
    Logs& operator=(const Logs&) = delete;

    ebisu_logger_t* get() {
        return &logger.base;
    }

    bool mentions(const std::string& part) const {
        for (const auto& l : lines) {
            if (l.second.find(part) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// An option's value, as lwip_getsockopt() gives it, or -1 if it gives none.
inline long option_of(int fd, int level, int name)
{
    int value = 0;
    socklen_t len = sizeof(value);
    if (lwip_getsockopt(fd, level, name, &value, &len) != 0) {
        return -1;
    }
    return value;
}

inline long elapsed_ms(std::chrono::steady_clock::time_point start)
{
    return (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
}

// A server on the loopback interface, at a port of its own, for one
// connection: 127.0.0.1, or ::1 with LWIP_IPV6.
struct Server {
    int listener;
    int peer;
    uint16_t port;

    explicit Server(int backlog = 1, bool ipv6 = false) : listener(-1), peer(-1), port(0) {
#if LWIP_IPV6
        if (ipv6) {
            listener = lwip_socket(AF_INET6, SOCK_STREAM, 0);
            REQUIRE( listener >= 0 );
            sockaddr_in6 addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin6_len = sizeof(addr);
            addr.sin6_family = AF_INET6;
            // ip6addr_aton(), as lwIP 2.0 has no lwip_inet_pton().
            ip6_addr_t loopback6;
            REQUIRE( ip6addr_aton("::1", &loopback6) == 1 );
            inet6_addr_from_ip6addr(&addr.sin6_addr, &loopback6);
            REQUIRE( lwip_bind(listener, (sockaddr*)&addr, sizeof(addr)) == 0 );
            socklen_t len = sizeof(addr);
            REQUIRE( lwip_getsockname(listener, (sockaddr*)&addr, &len) == 0 );
            port = lwip_ntohs(addr.sin6_port);
            REQUIRE( lwip_listen(listener, backlog) == 0 );
            return;
        }
#else
        (void)ipv6;
#endif
        listener = lwip_socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE( listener >= 0 );
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_len = sizeof(addr);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = lwip_htonl(0x7f000001u);
        REQUIRE( lwip_bind(listener, (sockaddr*)&addr, sizeof(addr)) == 0 );
        socklen_t len = sizeof(addr);
        REQUIRE( lwip_getsockname(listener, (sockaddr*)&addr, &len) == 0 );
        port = lwip_ntohs(addr.sin_port);
        REQUIRE( lwip_listen(listener, backlog) == 0 );
    }

    ~Server() {
        if (peer >= 0) {
            lwip_close(peer);
        }
        lwip_close(listener);
    }

    // Takes the connection the socket made, which the backlog kept, within 5
    // seconds: lwIP's accept waits for the listener's receive timeout.
    void accept_one() {
#if TEST_TIMEOUT_MS
        int timeout = 5000;
#else
        timeval timeout = { 5, 0 };
#endif
        REQUIRE( lwip_setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                    sizeof(timeout)) == 0 );
        peer = lwip_accept(listener, nullptr, nullptr);
        REQUIRE( peer >= 0 );
    }

    void send_text(const std::string& text) {
        REQUIRE( lwip_send(peer, text.data(), text.size(), 0) == (int)text.size() );
    }

    // As much as length, or less if the connection ends first.
    std::string receive(size_t length) {
        std::string got;
        while (got.size() < length) {
            char buffer[256];
            int received = (int)lwip_recv(peer, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            got.append(buffer, (size_t)received);
        }
        return got;
    }

    // The end of the stream.
    void close_peer() {
        lwip_close(peer);
        peer = -1;
    }

    // Waits for the connection to have something to read, with select, or
    // poll where lwIP is built without select, as the socket waits.
    void wait_readable() {
#if LWIP_VERSION_MAJOR < 2 || (LWIP_VERSION_MAJOR == 2 && LWIP_VERSION_MINOR == 0) || \
    LWIP_SOCKET_SELECT
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(peer, &readable);
        timeval timeout = { 5, 0 };
        REQUIRE( lwip_select(peer + 1, &readable, nullptr, nullptr, &timeout) == 1 );
#else
        pollfd p;
        p.fd = peer;
        p.events = POLLIN;
        p.revents = 0;
        REQUIRE( lwip_poll(&p, 1, 5000) == 1 );
#endif
    }

    // A reset, of a connection the socket has sent something on: closed with
    // SO_LINGER's timeout at 0, which lwIP has abort the connection, with RST.
    // lwIP 1.4 has no SO_LINGER, and its close sends FIN whatever was left
    // unread: the connection's pcb on this side, at the server's port, is
    // aborted in lwIP's core instead, which sends the RST.
    void reset_peer() {
        wait_readable();
#if LWIP_VERSION_MAJOR < 2
        bool aborted = false;
        LOCK_TCPIP_CORE();
        for (tcp_pcb* pcb = tcp_active_pcbs; pcb != nullptr; pcb = pcb->next) {
            if (pcb->local_port == port) {
                tcp_abort(pcb);
                aborted = true;
                break;
            }
        }
        UNLOCK_TCPIP_CORE();
        REQUIRE( aborted );
#else
        linger off;
        off.l_onoff = 1;
        off.l_linger = 0;
        REQUIRE( lwip_setsockopt(peer, SOL_SOCKET, SO_LINGER, &off, sizeof(off)) == 0 );
#endif
        close_peer();
    }
};

// A loopback port nothing listens on: one just bound and let go.
inline uint16_t closed_port()
{
    Server server;
    return server.port;
}

// lwIP's DNS client's servers, set for a test, and none again after it, as the
// other tests have them: a name not in the local host list then fails at
// once, but in lwIP 1.4, whose client asserts that it has one.
struct Servers {
    explicit Servers(const char* first, const char* second = nullptr) {
        set(0, first);
        set(1, second);
    }
    ~Servers() {
        set(0, nullptr);
        set(1, nullptr);
    }
    static void set(u8_t index, const char* address) {
        ip_addr_t addr;
        if (address != nullptr) {
            REQUIRE( ipaddr_aton(address, &addr) == 1 );
        }
        LOCK_TCPIP_CORE();
        dns_setserver(index, address != nullptr ? &addr : nullptr);
        UNLOCK_TCPIP_CORE();
    }
};

// What a name server answers: the query, as a response, with the name not
// found, which the probe counts as an answer.
std::string response_to(const std::string& query)
{
    std::string response = query;
    if (response.size() >= 4) {
        response[2] = (char)(response[2] | 0x80);
        response[3] = (char)((response[3] & 0xf0) | 3);
    }
    return response;
}

// A name server on 127.0.0.1 of lwIP's, at the port given, 0 for one of its
// own: each query it gets, it answers as told.
struct Responder {
    enum Mode {
        ANSWER,            // the response
        NOISE_FIRST,       // a datagram that isn't it, then the response
        FROM_ELSEWHERE,    // the response, from another port
        LATE_NOISE         // a datagram that isn't it, 900 ms after, and nothing else
    };
    int sock;
    int other;
    uint16_t port;
    Mode mode;
    std::atomic<bool> stop;
    std::atomic<int> queries;
    std::thread thread;

    explicit Responder(uint16_t at = 0, Mode m = ANSWER)
        : sock(-1), other(-1), port(0), mode(m), stop(false), queries(0) {
        sock = bound(at);
        other = bound(0);
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        REQUIRE( lwip_getsockname(sock, (sockaddr*)&addr, &len) == 0 );
        port = lwip_ntohs(addr.sin_port);
#if TEST_TIMEOUT_MS
        int timeout = 100;
#else
        timeval timeout = { 0, 100000 };
#endif
        REQUIRE( lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 );
        thread = std::thread([this] { serve(); });
    }

    ~Responder() {
        stop = true;
        thread.join();
        lwip_close(other);
        lwip_close(sock);
    }

    static int bound(uint16_t at) {
        int s = lwip_socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE( s >= 0 );
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_len = sizeof(addr);
        addr.sin_family = AF_INET;
        addr.sin_port = lwip_htons(at);
        addr.sin_addr.s_addr = lwip_htonl(0x7f000001u);
        REQUIRE( lwip_bind(s, (sockaddr*)&addr, sizeof(addr)) == 0 );
        return s;
    }

    void serve() {
        while (!stop) {
            char buffer[512];
            sockaddr_in from;
            socklen_t len = sizeof(from);
            int got = (int)lwip_recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&from, &len);
            if (got <= 0) {
                continue;
            }
            ++queries;
            if (mode == LATE_NOISE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(900));
                lwip_sendto(sock, "noise", 5, 0, (sockaddr*)&from, len);
                continue;
            }
            std::string response = response_to(std::string(buffer, (size_t)got));
            if (mode == NOISE_FIRST) {
                lwip_sendto(sock, "noise", 5, 0, (sockaddr*)&from, len);
            }
            lwip_sendto(mode == FROM_ELSEWHERE ? other : sock, response.data(), response.size(), 0,
                    (sockaddr*)&from, len);
        }
    }
};

}

#endif // EBISU_LWIP_TEST_H
