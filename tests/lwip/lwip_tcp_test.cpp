// platform's socket of plain TCP over lwIP's sockets, against servers of the
// test's own, on lwIP's sockets too, on its loopback interface: lwIP runs in
// this process, on a port to POSIX threads, and reaches no other host. An
// interface that drops what it is given stands for an address that doesn't
// answer. See README.md.

#define CATCH_CONFIG_RUNNER
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
long option_of(int fd, int level, int name)
{
    int value = 0;
    socklen_t len = sizeof(value);
    if (lwip_getsockopt(fd, level, name, &value, &len) != 0) {
        return -1;
    }
    return value;
}

long elapsed_ms(std::chrono::steady_clock::time_point start)
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
uint16_t closed_port()
{
    Server server;
    return server.port;
}

// The socket, connected to a server that took the connection.
struct Connected {
    Server server;
    ebisu_lwip_tcp_socket_t sock;
    const ebisu_socket_ops_t* ops;

    explicit Connected(unsigned int to_recv = 1) {
        ebisu_lwip_tcp_socket_init(&sock, to_recv, 5);
        ops = sock.base.ops;
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
        server.accept_one();
    }

    ~Connected() {
        ops->close(&sock.base);
    }

    std::string receive(size_t length, ebisu_sock_code_t* out_code = nullptr) {
        std::vector<char> buffer(length);
        size_t received = 0;
        ebisu_sock_code_t code = ops->recv(&sock.base, buffer.data(), length, &received);
        if (out_code != nullptr) {
            *out_code = code;
        }
        return std::string(buffer.data(), received);
    }

    ebisu_sock_error_t last_error(long* detail) {
        return ops->last_error(&sock.base, detail);
    }
};

bool is_gone(long detail)
{
    return detail == ECONNRESET || detail == ENOTCONN || detail == ECONNABORTED ||
        detail == EPIPE;
}

// The interface that drops all it is given, on 10.255.0.1/24: a connection to
// 10.255.0.2 is never answered.
const char* const SILENT = "10.255.0.2";
struct netif silent_netif;

err_t drop(struct netif* netif, struct pbuf* p, const ip4_addr_t* addr)
{
    (void)netif;
    (void)p;
    (void)addr;
    return ERR_OK;
}

#if LWIP_IPV6
err_t drop6(struct netif* netif, struct pbuf* p, const ip6_addr_t* addr)
{
    (void)netif;
    (void)p;
    (void)addr;
    return ERR_OK;
}
#endif

err_t silent_init(struct netif* netif)
{
    netif->name[0] = 's';
    netif->name[1] = 'i';
    netif->mtu = 1500;
    netif->output = drop;
#if LWIP_IPV6
    netif->output_ip6 = drop6;
#endif
    return ERR_OK;
}

void started(void* arg)
{
    sys_sem_signal(static_cast<sys_sem_t*>(arg));
}

// lwIP, started once for all the tests, with the interface that drops, and
// the names they look up.
void start_lwip()
{
    // Before Catch's session, so without its REQUIRE.
    sys_sem_t done;
    if (sys_sem_new(&done, 0) != ERR_OK) {
        fprintf(stderr, "failed to start lwIP\n");
        exit(2);
    }
    tcpip_init(started, &done);
    sys_arch_sem_wait(&done, 0);
    sys_sem_free(&done);

    LOCK_TCPIP_CORE();
    ip4_addr_t addr;
    ip4_addr_t mask;
    ip4_addr_t gw;
    IP4_ADDR(&addr, 10, 255, 0, 1);
    IP4_ADDR(&mask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 0, 0, 0, 0);
    netif_add(&silent_netif, &addr, &mask, &gw, nullptr, silent_init, tcpip_input);
    netif_set_up(&silent_netif);
    netif_set_link_up(&silent_netif);
    ip_addr_t loopback;
    ipaddr_aton("127.0.0.1", &loopback);
    dns_local_addhost("server.test", &loopback);
    UNLOCK_TCPIP_CORE();
}

}

int main(int argc, char* argv[])
{
    start_lwip();
    return Catch::Session().run(argc, argv);
}

TEST_CASE( "The lwIP socket sends and receives over the connection it makes" ) {
    Connected c;
    size_t sent = 0;
    REQUIRE( c.ops->send(&c.sock.base, "hello", 5, &sent) == EBISU_SOCK_OK );
    CHECK( sent == 5 );
    CHECK( c.server.receive(5) == "hello" );
    c.server.send_text("world");
    ebisu_sock_code_t code = EBISU_SOCK_FAIL;
    CHECK( c.receive(16, &code) == "world" );
    CHECK( code == EBISU_SOCK_OK );
    CHECK_FALSE( c.ops->recv_timed_out(&c.sock.base) );

    SECTION("and connects again once closed") {
        REQUIRE( c.ops->close(&c.sock.base) == EBISU_SOCK_OK );
        CHECK( c.sock._socket == -1 );
        Server again;
        REQUIRE( c.ops->connect(&c.sock.base, "127.0.0.1", again.port) == EBISU_SOCK_OK );
        again.accept_one();
        REQUIRE( c.ops->send(&c.sock.base, "again", 5, &sent) == EBISU_SOCK_OK );
        CHECK( again.receive(5) == "again" );
    }
}

TEST_CASE( "The lwIP socket looks a name up, and takes an address as written" ) {
    Server server;
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;

    SECTION("a name, which lwIP's DNS client resolves") {
        REQUIRE( ops->connect(&sock.base, "server.test", server.port) == EBISU_SOCK_OK );
    }
    SECTION("an address, without asking") {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
    }
    server.accept_one();
    size_t sent = 0;
    REQUIRE( ops->send(&sock.base, "named", 5, &sent) == EBISU_SOCK_OK );
    CHECK( server.receive(5) == "named" );
    ops->close(&sock.base);
}

#if LWIP_IPV6
TEST_CASE( "The lwIP socket connects over IPv6" ) {
    Server server(1, true);
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    REQUIRE( ops->connect(&sock.base, "::1", server.port) == EBISU_SOCK_OK );
    server.accept_one();
    size_t sent = 0;
    REQUIRE( ops->send(&sock.base, "six", 3, &sent) == EBISU_SOCK_OK );
    CHECK( server.receive(3) == "six" );
    ops->close(&sock.base);
}
#endif

// The MQTT task's socket times out on every idle stretch, and goes on with the
// connection: the timeout says so, and loses nothing.
TEST_CASE( "The lwIP socket's receive times out, says so, and leaves the connection as it was" ) {
    Connected c(1);
    auto start = std::chrono::steady_clock::now();
    ebisu_sock_code_t code = EBISU_SOCK_OK;
    CHECK( c.receive(16, &code).empty() );
    CHECK( code == EBISU_SOCK_FAIL );
    long ms = elapsed_ms(start);
    CHECK( ms >= 900 );
    CHECK( ms < 5000 );
    CHECK( c.ops->recv_timed_out(&c.sock.base) );
    long detail = -1;
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_TIMEOUT );
    CHECK( detail == 0 );

    c.server.send_text("after");
    CHECK( c.receive(16, &code) == "after" );
    CHECK( code == EBISU_SOCK_OK );
    CHECK_FALSE( c.ops->recv_timed_out(&c.sock.base) );
}

TEST_CASE( "The lwIP socket takes a peer's close for the end of the stream" ) {
    Connected c;
    c.server.send_text("last");
    c.server.close_peer();
    ebisu_sock_code_t code = EBISU_SOCK_FAIL;
    CHECK( c.receive(16, &code) == "last" );
    CHECK( code == EBISU_SOCK_OK );
    CHECK( c.receive(16, &code).empty() );
    CHECK( code == EBISU_SOCK_OK );
}

TEST_CASE( "The lwIP socket says a peer reset the connection" ) {
    Connected c;
    size_t sent = 0;
    REQUIRE( c.ops->send(&c.sock.base, "x", 1, &sent) == EBISU_SOCK_OK );
    c.server.reset_peer();
    ebisu_sock_code_t code = EBISU_SOCK_OK;
    CHECK( c.receive(16, &code).empty() );
    long detail = 0;
#if LWIP_VERSION_MAJOR == 2 && LWIP_VERSION_MINOR == 0
    // lwIP 2.0's receive takes a reset for the end of the stream, and the
    // sends after it fail.
    REQUIRE( code == EBISU_SOCK_OK );
    const std::string data(1024, 'y');
    for (int i = 0; i < 100 && code == EBISU_SOCK_OK; ++i) {
        code = c.ops->send(&c.sock.base, data.data(), data.size(), &sent);
    }
    REQUIRE( code == EBISU_SOCK_FAIL );
#else
    REQUIRE( code == EBISU_SOCK_FAIL );
    CHECK_FALSE( c.ops->recv_timed_out(&c.sock.base) );
#endif
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_CLOSED );
    INFO( "errno " << detail );
    CHECK( is_gone(detail) );
}

TEST_CASE( "The lwIP socket's send to a peer that has gone fails" ) {
    Connected c;
    size_t sent = 0;
    REQUIRE( c.ops->send(&c.sock.base, "x", 1, &sent) == EBISU_SOCK_OK );
    c.server.reset_peer();
    const std::string data(1024, 'x');
    ebisu_sock_code_t code = EBISU_SOCK_OK;
    // The first send may go before the reset has arrived.
    for (int i = 0; i < 100 && code == EBISU_SOCK_OK; ++i) {
        code = c.ops->send(&c.sock.base, data.data(), data.size(), &sent);
    }
    REQUIRE( code == EBISU_SOCK_FAIL );
    long detail = 0;
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_CLOSED );
    INFO( "errno " << detail );
    CHECK( is_gone(detail) );

    CHECK( c.ops->send(&c.sock.base, data.data(), data.size(), &sent) == EBISU_SOCK_FAIL );
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_CLOSED );
}

TEST_CASE( "The lwIP socket's recv_wait waits for something for recv to return" ) {
    Connected c(5);
    REQUIRE( c.ops->recv_wait != nullptr );

    SECTION("for its timeout, with nothing arriving") {
        auto start = std::chrono::steady_clock::now();
        CHECK_FALSE( c.ops->recv_wait(&c.sock.base, 200) );
        long ms = elapsed_ms(start);
        CHECK( ms >= 150 );
        CHECK( ms < 3000 );
    }

    SECTION("not at all, for none") {
        CHECK_FALSE( c.ops->recv_wait(&c.sock.base, 0) );
    }

    SECTION("not for data, which recv then returns") {
        c.server.send_text("data");
        CHECK( c.ops->recv_wait(&c.sock.base, 5000) );
        CHECK( c.receive(16) == "data" );
    }

    SECTION("not for the end of the stream") {
        c.server.close_peer();
        CHECK( c.ops->recv_wait(&c.sock.base, 5000) );
        ebisu_sock_code_t code = EBISU_SOCK_FAIL;
        CHECK( c.receive(16, &code).empty() );
        CHECK( code == EBISU_SOCK_OK );
    }
}

TEST_CASE( "The lwIP socket says why it failed to connect" ) {
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    long detail = -1;

    SECTION("a host that doesn't resolve, with lwip_getaddrinfo()'s error") {
        for (unsigned int timeout : { 5u, 0u }) {
            INFO( "connect timeout " << timeout );
            ebisu_lwip_tcp_socket_set_connect_timeout(&sock, timeout);
            REQUIRE( ops->connect(&sock.base, "missing.test", 1883) == EBISU_SOCK_FAIL );
            CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS );
            CHECK( detail == EAI_FAIL );
        }
    }

    SECTION("an address that refuses, with lwIP's errno") {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", closed_port()) == EBISU_SOCK_FAIL );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_CONNECT );
        INFO( "errno " << detail );
        // lwIP says a refusal as the reset it is.
        CHECK( detail == ECONNRESET );
    }

    SECTION("an address that doesn't answer within the connect timeout") {
        ebisu_lwip_tcp_socket_set_connect_timeout(&sock, 1);
        auto start = std::chrono::steady_clock::now();
        REQUIRE( ops->connect(&sock.base, SILENT, 1883) == EBISU_SOCK_FAIL );
        long ms = elapsed_ms(start);
        CHECK( ms >= 900 );
        CHECK( ms < 5000 );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK( detail == ETIMEDOUT );
        CHECK( sock._socket == -1 );
    }
}

TEST_CASE( "The lwIP socket says why it failed to its logger" ) {
    Logs logs;
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    ebisu_lwip_tcp_socket_set_logger(&sock, logs.get());
    const ebisu_socket_ops_t* ops = sock.base.ops;

    SECTION("a host that doesn't resolve") {
        REQUIRE( ops->connect(&sock.base, "missing.test", 1883) == EBISU_SOCK_FAIL );
#ifdef EBISU_NO_LOG
        CHECK( logs.lines.empty() );
#else
        REQUIRE( logs.lines.size() == 1 );
        CHECK( logs.lines[0].first == EBISU_LOG_ERROR );
        CHECK( logs.mentions("failed to look up missing.test") );
#endif
    }

    SECTION("an address that refuses") {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", closed_port()) == EBISU_SOCK_FAIL );
#ifdef EBISU_NO_LOG
        CHECK( logs.lines.empty() );
#else
        REQUIRE( logs.lines.size() == 1 );
        CHECK( logs.lines[0].first == EBISU_LOG_ERROR );
        CHECK( logs.mentions("failed to connect to 127.0.0.1") );
#endif
    }

    SECTION("but not a receive that times out, which is routine") {
        Server server;
        ebisu_lwip_tcp_socket_init(&sock, 1, 5);
        ebisu_lwip_tcp_socket_set_logger(&sock, logs.get());
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
        server.accept_one();
        char buffer[16];
        size_t received = 0;
        REQUIRE( ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_FAIL );
        CHECK( ops->recv_timed_out(&sock.base) );
        CHECK( logs.lines.empty() );
        ops->close(&sock.base);
    }
}

// For a connection held for hours, with a request now and then: keepalive
// notices a peer that has gone while the connection is idle, and TCP_NODELAY
// sends a request in two pieces without waiting for the first's ACK.
TEST_CASE( "The lwIP socket checks an idle connection, and sends at once, when asked" ) {
    Logs logs;
    Server server(4);
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    ebisu_lwip_tcp_socket_set_logger(&sock, logs.get());
    const ebisu_socket_ops_t* ops = sock.base.ops;
    auto connect = [&] {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
        if (server.peer >= 0) {
            server.close_peer();
        }
        server.accept_one();
        return sock._socket;
    };

    SECTION("neither, as it starts") {
        int fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
        CHECK( logs.lines.empty() );
    }
    SECTION("both, from the next connect") {
        int fd = connect();
        ebisu_lwip_tcp_socket_set_keepalive(&sock, 30, 5, 4);
        ebisu_lwip_tcp_socket_set_nodelay(&sock, true);
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
        REQUIRE( ops->close(&sock.base) == EBISU_SOCK_OK );

        fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) != 0 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) != 0 );
#if LWIP_TCP_KEEPALIVE
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPIDLE) == 30 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPINTVL) == 5 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPCNT) == 4 );
        CHECK( logs.lines.empty() );
#elif defined(EBISU_NO_LOG)
        CHECK( logs.lines.empty() );
#else
        // Without LWIP_TCP_KEEPALIVE, lwIP's own idle time, interval and
        // count, which the socket says.
        REQUIRE( logs.lines.size() == 1 );
        CHECK( logs.lines[0].first == EBISU_LOG_WARNING );
        CHECK( logs.mentions("without LWIP_TCP_KEEPALIVE") );
#endif
        size_t sent = 0;
        REQUIRE( ops->send(&sock.base, "ping", 4, &sent) == EBISU_SOCK_OK );
        CHECK( server.receive(4) == "ping" );

        SECTION("and neither again, once off") {
            REQUIRE( ops->close(&sock.base) == EBISU_SOCK_OK );
            ebisu_lwip_tcp_socket_set_keepalive(&sock, 0, 5, 4);
            ebisu_lwip_tcp_socket_set_nodelay(&sock, false);
            fd = connect();
            CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
            CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
        }
    }
#if LWIP_TCP_KEEPALIVE
    SECTION("keepalive with lwIP's interval and count") {
        ebisu_lwip_tcp_socket_set_keepalive(&sock, 60, 0, 0);
        int fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) != 0 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPIDLE) == 60 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPINTVL) == TCP_KEEPINTVL_DEFAULT / 1000 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPCNT) == TCP_KEEPCNT_DEFAULT );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
    }
#endif
    ops->close(&sock.base);
}

// A receive timeout as lwIP is built to take it: an int of milliseconds with
// LWIP_SO_SNDRCVTIMEO_NONSTANDARD, a struct timeval otherwise.
TEST_CASE( "The lwIP socket sets its timeouts as lwIP takes them" ) {
    Server server;
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 7, 3);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
    server.accept_one();
#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
    int recv_ms = 0;
    int send_ms = 0;
    socklen_t len = sizeof(int);
    REQUIRE( lwip_getsockopt(sock._socket, SOL_SOCKET, SO_RCVTIMEO, &recv_ms, &len) == 0 );
    REQUIRE( lwip_getsockopt(sock._socket, SOL_SOCKET, SO_SNDTIMEO, &send_ms, &len) == 0 );
    CHECK( recv_ms == 7000 );
    CHECK( send_ms == 3000 );
#else
    timeval recv_tv = { 0, 0 };
    timeval send_tv = { 0, 0 };
    socklen_t len = sizeof(timeval);
    REQUIRE( lwip_getsockopt(sock._socket, SOL_SOCKET, SO_RCVTIMEO, &recv_tv, &len) == 0 );
    REQUIRE( lwip_getsockopt(sock._socket, SOL_SOCKET, SO_SNDTIMEO, &send_tv, &len) == 0 );
    CHECK( recv_tv.tv_sec == 7 );
    CHECK( send_tv.tv_sec == 3 );
#endif
    ops->close(&sock.base);
}
