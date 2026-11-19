// What the lwIP port's tests share: lwIP's headers, a logger that keeps its
// lines, and servers of the test's own on lwIP's loopback interface. See
// README.md.
#ifndef EBISU_LWIP_TEST_H
#define EBISU_LWIP_TEST_H

#include "catch.hpp"

#include <ebisu_lwip.h>

#include "lwip/opt.h"
/* Before lwip/dns.h, which lwIP 2.0 has use err_t without it. */
#include "lwip/err.h"
#include "lwip/dns.h"
#include "lwip/inet.h"
#include "lwip/errno.h"
#include "lwip/init.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
/* For TCP_KEEPINTVL_DEFAULT and TCP_KEEPCNT_DEFAULT. */
#include "lwip/priv/tcp_priv.h"
#include "lwip/sockets.h"
#include "lwip/tcpip.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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
#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
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
#if (LWIP_VERSION_MAJOR == 2 && LWIP_VERSION_MINOR == 0) || LWIP_SOCKET_SELECT
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
    void reset_peer() {
        wait_readable();
        linger off;
        off.l_onoff = 1;
        off.l_linger = 0;
        REQUIRE( lwip_setsockopt(peer, SOL_SOCKET, SO_LINGER, &off, sizeof(off)) == 0 );
        close_peer();
    }
};

// A loopback port nothing listens on: one just bound and let go.
inline uint16_t closed_port()
{
    Server server;
    return server.port;
}

}

#endif // EBISU_LWIP_TEST_H
