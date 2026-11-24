// platform's sockets of plain TCP, against servers of the test's own on
// 127.0.0.1: the POSIX port's, and on Windows the Win32 port's. The two take
// the same tests, and differ in their error codes and in how they wait.
//
// Their lookups are the TLS ports' code, which the OpenSSL port's large tests
// and the Schannel port's test hold back and let through: these only look up a
// name that doesn't resolve.

#include "catch_amalgamated.hpp"
#include "test_callbacks.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ebisu_win32.h>
#else
#include <ebisu_posix.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace {

#ifdef _WIN32
typedef ebisu_win32_tcp_socket_t tcp_socket_t;
typedef SOCKET fd_t;
const fd_t NO_FD = INVALID_SOCKET;
const long REFUSED = WSAECONNREFUSED;

void tcp_socket_init(tcp_socket_t* sock, unsigned int to_recv, unsigned int to_send)
{
    ebisu_win32_tcp_socket_init(sock, to_recv, to_send);
}

void set_connect_timeout(tcp_socket_t* sock, unsigned int seconds)
{
    ebisu_win32_tcp_socket_set_connect_timeout(sock, seconds);
}

void set_logger(tcp_socket_t* sock, ebisu_logger_t* logger)
{
    ebisu_win32_tcp_socket_set_logger(sock, logger);
}

void set_keepalive(tcp_socket_t* sock, unsigned int idle_s, unsigned int interval_s,
        unsigned int count)
{
    ebisu_win32_tcp_socket_set_keepalive(sock, idle_s, interval_s, count);
}

void set_nodelay(tcp_socket_t* sock, bool nodelay)
{
    ebisu_win32_tcp_socket_set_nodelay(sock, nodelay);
}

fd_t fd_of(const tcp_socket_t* sock)
{
    return (SOCKET)sock->socket;
}

// An option's value, as getsockopt() gives it, or -1 if it gives none.
long option_of(fd_t fd, int level, int name)
{
    DWORD value = 0;
    int len = sizeof(value);
    if (getsockopt(fd, level, name, (char*)&value, &len) != 0) {
        return -1;
    }
    return (long)value;
}

void close_fd(fd_t fd)
{
    closesocket(fd);
}

// Winsock, started once for the run, as the application starts it.
void start_network()
{
    static struct Winsock {
        Winsock() {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
        }
        ~Winsock() {
            WSACleanup();
        }
    } winsock;
}

// How a peer that has gone makes a send or a receive fail.
bool is_gone(long detail)
{
    return detail == WSAECONNRESET || detail == WSAECONNABORTED;
}
#else
typedef ebisu_posix_tcp_socket_t tcp_socket_t;
typedef int fd_t;
const fd_t NO_FD = -1;
const long REFUSED = ECONNREFUSED;

void tcp_socket_init(tcp_socket_t* sock, unsigned int to_recv, unsigned int to_send)
{
    ebisu_posix_tcp_socket_init(sock, to_recv, to_send);
}

void set_connect_timeout(tcp_socket_t* sock, unsigned int seconds)
{
    ebisu_posix_tcp_socket_set_connect_timeout(sock, seconds);
}

void set_logger(tcp_socket_t* sock, ebisu_logger_t* logger)
{
    ebisu_posix_tcp_socket_set_logger(sock, logger);
}

void set_keepalive(tcp_socket_t* sock, unsigned int idle_s, unsigned int interval_s,
        unsigned int count)
{
    ebisu_posix_tcp_socket_set_keepalive(sock, idle_s, interval_s, count);
}

void set_nodelay(tcp_socket_t* sock, bool nodelay)
{
    ebisu_posix_tcp_socket_set_nodelay(sock, nodelay);
}

fd_t fd_of(const tcp_socket_t* sock)
{
    return sock->socket;
}

// An option's value, as getsockopt() gives it, or -1 if it gives none.
long option_of(fd_t fd, int level, int name)
{
    int value = 0;
    socklen_t len = sizeof(value);
    if (getsockopt(fd, level, name, &value, &len) != 0) {
        return -1;
    }
    return value;
}

void close_fd(fd_t fd)
{
    close(fd);
}

void start_network()
{
}

bool is_gone(long detail)
{
    return detail == ECONNRESET || detail == EPIPE;
}
#endif

sockaddr_in loopback(uint16_t port)
{
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    return addr;
}

// A server on 127.0.0.1, at a port of its own, for one connection.
struct Server {
    fd_t listener;
    fd_t peer;
    uint16_t port;

    explicit Server(int backlog = 1) : listener(NO_FD), peer(NO_FD), port(0) {
        start_network();
        listener = socket(AF_INET, SOCK_STREAM, 0);
        REQUIRE( listener != NO_FD );
        sockaddr_in addr = loopback(0);
        REQUIRE( bind(listener, (sockaddr*)&addr, sizeof(addr)) == 0 );
        socklen_t len = sizeof(addr);
        REQUIRE( getsockname(listener, (sockaddr*)&addr, &len) == 0 );
        port = ntohs(addr.sin_port);
        REQUIRE( listen(listener, backlog) == 0 );
    }

    ~Server() {
        if (peer != NO_FD) {
            close_fd(peer);
        }
        close_fd(listener);
    }

    // Takes the connection the socket made, which the backlog kept.
    void accept_one() {
        peer = accept(listener, nullptr, nullptr);
        REQUIRE( peer != NO_FD );
    }

    void send_text(const std::string& text) {
        REQUIRE( send(peer, text.data(), (int)text.size(), 0) == (int)text.size() );
    }

    // As much as length, or less if the connection ends first.
    std::string receive(size_t length) {
        std::string got;
        while (got.size() < length) {
            char buffer[256];
            int received = recv(peer, buffer, (int)sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            got.append(buffer, (size_t)received);
        }
        return got;
    }

    // The end of the stream.
    void close_peer() {
        close_fd(peer);
        peer = NO_FD;
    }

    // Waits for the connection to have something to read.
    void wait_readable() {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(peer, &readable);
        timeval timeout = { 5, 0 };
        REQUIRE( select((int)peer + 1, &readable, nullptr, nullptr, &timeout) == 1 );
    }

    // A reset, of a connection the socket has sent something on: closed with
    // SO_LINGER's timeout at 0, which sends RST, or where that can't be set,
    // as under qemu-arm, closed with what was sent unread, which sends RST
    // too.
    void reset_peer() {
        wait_readable();
        linger off;
        off.l_onoff = 1;
        off.l_linger = 0;
        setsockopt(peer, SOL_SOCKET, SO_LINGER, (const char*)&off, sizeof(off));
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
    tcp_socket_t sock;
    const ebisu_socket_ops_t* ops;

    explicit Connected(unsigned int to_recv = 1) {
        tcp_socket_init(&sock, to_recv, 5);
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

long elapsed_ms(std::chrono::steady_clock::time_point start)
{
    return (long)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
}

}

TEST_CASE( "The plain TCP socket sends and receives over the connection it makes" ) {
    Connected c;
    size_t sent = 0;
    REQUIRE( c.ops->send(&c.sock.base, "hello", 5, &sent) == EBISU_SOCK_OK );
    CHECK( sent == 5 );
    CHECK( c.server.receive(5) == "hello" );
    c.server.send_text("world");
    ebisu_sock_code_t code = EBISU_SOCK_FAIL;
    CHECK( c.receive(16, &code) == "world" );
    CHECK( code == EBISU_SOCK_OK );

    SECTION("and connects again once closed") {
        REQUIRE( c.ops->close(&c.sock.base) == EBISU_SOCK_OK );
        Server again;
        REQUIRE( c.ops->connect(&c.sock.base, "127.0.0.1", again.port) == EBISU_SOCK_OK );
        again.accept_one();
        REQUIRE( c.ops->send(&c.sock.base, "again", 5, &sent) == EBISU_SOCK_OK );
        CHECK( again.receive(5) == "again" );
    }
}

// The MQTT task's socket times out on every idle stretch, and goes on with the
// connection: the timeout says so, and loses nothing.
TEST_CASE( "The plain TCP socket's receive times out, says so, and leaves the connection as it was" ) {
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
}

TEST_CASE( "The plain TCP socket takes a peer's close for the end of the stream" ) {
    Connected c;
    c.server.send_text("last");
    c.server.close_peer();
    ebisu_sock_code_t code = EBISU_SOCK_FAIL;
    CHECK( c.receive(16, &code) == "last" );
    CHECK( code == EBISU_SOCK_OK );
    CHECK( c.receive(16, &code).empty() );
    CHECK( code == EBISU_SOCK_OK );
}

TEST_CASE( "The plain TCP socket says a peer reset the connection" ) {
    Connected c;
    size_t sent = 0;
    REQUIRE( c.ops->send(&c.sock.base, "x", 1, &sent) == EBISU_SOCK_OK );
    c.server.reset_peer();
    ebisu_sock_code_t code = EBISU_SOCK_OK;
    CHECK( c.receive(16, &code).empty() );
    REQUIRE( code == EBISU_SOCK_FAIL );
    CHECK_FALSE( c.ops->recv_timed_out(&c.sock.base) );
    long detail = 0;
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_CLOSED );
    CHECK( is_gone(detail) );
}

// Without MSG_NOSIGNAL or SO_NOSIGPIPE, a send to a peer that has gone raises
// SIGPIPE, which ends the program: this one doesn't ignore it. On Linux the
// send that finds the reset fails with ECONNRESET, and the next with EPIPE,
// which is the one that raises it.
TEST_CASE( "The plain TCP socket's send to a peer that has gone fails" ) {
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
    CHECK( is_gone(detail) );

    CHECK( c.ops->send(&c.sock.base, data.data(), data.size(), &sent) == EBISU_SOCK_FAIL );
    CHECK( c.last_error(&detail) == EBISU_SOCK_ERROR_CLOSED );
}

TEST_CASE( "The plain TCP socket's recv_wait waits for something for recv to return" ) {
    Connected c(5);
    REQUIRE( c.ops->recv_wait != nullptr );

    SECTION("for its timeout, with nothing arriving") {
        auto start = std::chrono::steady_clock::now();
        CHECK_FALSE( c.ops->recv_wait(&c.sock.base, 200) );
        long ms = elapsed_ms(start);
        CHECK( ms >= 150 );
        CHECK( ms < 3000 );
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

TEST_CASE( "The plain TCP socket says why it failed to connect" ) {
    start_network();
    tcp_socket_t sock;
    tcp_socket_init(&sock, 5, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    long detail = -1;

    SECTION("a host that doesn't resolve, with the resolver's error") {
        for (unsigned int timeout : { 5u, 0u }) {
            INFO( "connect timeout " << timeout );
            set_connect_timeout(&sock, timeout);
            REQUIRE( ops->connect(&sock.base, "ebisu-test.invalid", 1883) == EBISU_SOCK_FAIL );
            CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS );
            CHECK( detail != 0 );
        }
    }

    SECTION("an address that refuses, with the system's error") {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", closed_port()) == EBISU_SOCK_FAIL );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_CONNECT );
        CHECK( detail == REFUSED );
    }

    // A listener with a full backlog drops the SYNs of another connection, as
    // an address that doesn't answer does.
    SECTION("an address that doesn't answer within the connect timeout") {
        Server full(0);
        std::vector<fd_t> fillers;
        for (int i = 0; i < 2; ++i) {
            fd_t filler = socket(AF_INET, SOCK_STREAM, 0);
            REQUIRE( filler != NO_FD );
#ifdef _WIN32
            u_long nonblocking = 1;
            ioctlsocket(filler, FIONBIO, &nonblocking);
#else
            fcntl(filler, F_SETFL, fcntl(filler, F_GETFL, 0) | O_NONBLOCK);
#endif
            sockaddr_in addr = loopback(full.port);
            connect(filler, (sockaddr*)&addr, sizeof(addr));
            fillers.push_back(filler);
        }
        set_connect_timeout(&sock, 1);
        auto start = std::chrono::steady_clock::now();
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", full.port) == EBISU_SOCK_FAIL );
        long ms = elapsed_ms(start);
        CHECK( ms >= 900 );
        CHECK( ms < 5000 );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        for (fd_t filler : fillers) {
            close_fd(filler);
        }
    }
}

TEST_CASE( "The plain TCP socket says why it failed to its logger" ) {
    start_network();
    khct::cb::LoggerCtx logs;
    tcp_socket_t sock;
    tcp_socket_init(&sock, 5, 5);
    set_logger(&sock, logs.get());
    const ebisu_socket_ops_t* ops = sock.base.ops;

    SECTION("a host that doesn't resolve") {
        REQUIRE( ops->connect(&sock.base, "ebisu-test.invalid", 1883) == EBISU_SOCK_FAIL );
#ifdef EBISU_NO_LOG
        CHECK( logs.lines.empty() );
#else
        REQUIRE( logs.lines.size() == 1 );
        CHECK( logs.lines[0].first == EBISU_LOG_ERROR );
        CHECK( logs.mentions("failed to look up ebisu-test.invalid") );
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
        tcp_socket_init(&sock, 1, 5);
        set_logger(&sock, logs.get());
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
TEST_CASE( "The plain TCP socket checks an idle connection, and sends at once, when asked" ) {
    start_network();
    khct::cb::LoggerCtx logs;
    Server server(4);
    tcp_socket_t sock;
    tcp_socket_init(&sock, 5, 5);
    set_logger(&sock, logs.get());
    const ebisu_socket_ops_t* ops = sock.base.ops;
    auto connect = [&] {
        REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
        if (server.peer != NO_FD) {
            server.close_peer();
        }
        server.accept_one();
        return fd_of(&sock);
    };
#if defined(TCP_KEEPIDLE)
    const int idle_option = TCP_KEEPIDLE;
#elif defined(TCP_KEEPALIVE)
    const int idle_option = TCP_KEEPALIVE;
#endif
    // What a socket has without them.
    fd_t fresh = socket(AF_INET, SOCK_STREAM, 0);
    REQUIRE( fresh != NO_FD );
    const long system_interval = option_of(fresh, IPPROTO_TCP, TCP_KEEPINTVL);
    const long system_count = option_of(fresh, IPPROTO_TCP, TCP_KEEPCNT);
    close_fd(fresh);

    SECTION("neither, as it starts") {
        fd_t fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
    }
    SECTION("both, from the next connect") {
        fd_t fd = connect();
        set_keepalive(&sock, 30, 5, 4);
        set_nodelay(&sock, true);
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
        REQUIRE( ops->close(&sock.base) == EBISU_SOCK_OK );

        fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) != 0 );
        CHECK( option_of(fd, IPPROTO_TCP, idle_option) == 30 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPINTVL) == 5 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPCNT) == 4 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) != 0 );
        size_t sent = 0;
        REQUIRE( ops->send(&sock.base, "ping", 4, &sent) == EBISU_SOCK_OK );
        CHECK( server.receive(4) == "ping" );
        CHECK( logs.lines.empty() );

        SECTION("and neither again, once off") {
            REQUIRE( ops->close(&sock.base) == EBISU_SOCK_OK );
            set_keepalive(&sock, 0, 5, 4);
            set_nodelay(&sock, false);
            fd = connect();
            CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) == 0 );
            CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
        }
    }
    SECTION("keepalive with the system's interval and count") {
        set_keepalive(&sock, 60, 0, 0);
        fd_t fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) != 0 );
        CHECK( option_of(fd, IPPROTO_TCP, idle_option) == 60 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPINTVL) == system_interval );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPCNT) == system_count );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_NODELAY) == 0 );
    }
#ifdef __linux__
    SECTION("a value the system refuses is said, and the connection made without it") {
        // Linux takes a count of 127 at most.
        set_keepalive(&sock, 30, 5, 200);
        fd_t fd = connect();
        CHECK( option_of(fd, SOL_SOCKET, SO_KEEPALIVE) != 0 );
        CHECK( option_of(fd, IPPROTO_TCP, idle_option) == 30 );
        CHECK( option_of(fd, IPPROTO_TCP, TCP_KEEPCNT) == system_count );
#ifdef EBISU_NO_LOG
        CHECK( logs.lines.empty() );
#else
        REQUIRE( logs.lines.size() == 1 );
        CHECK( logs.lines[0].first == EBISU_LOG_WARNING );
        CHECK( logs.mentions("failed to set TCP_KEEPCNT to 200") );
#endif
    }
#endif
}

namespace {

// The families "localhost" is looked up as, in the order the sockets' lookup
// gives them, with the same hints.
std::vector<int> localhost_families()
{
    start_network();
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* result = nullptr;
    std::vector<int> families;
    if (getaddrinfo("localhost", "80", &hints, &result) != 0) {
        return families;
    }
    for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
        families.push_back(ai->ai_family);
    }
    freeaddrinfo(result);
    return families;
}

// A server on the loopback address of a family, 127.0.0.1 or ::1, at a port,
// or at one of its own with port 0, which may be taken already.
struct FamilyServer {
    fd_t listener;
    uint16_t port;

    FamilyServer(int family, uint16_t at) : listener(NO_FD), port(0) {
        listener = socket(family, SOCK_STREAM, 0);
        if (listener == NO_FD) {
            return;
        }
        int on = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
        sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        socklen_t len;
        if (family == AF_INET6) {
            // Only ::1, and not 127.0.0.1 as well, as a mapped address.
            setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on));
            sockaddr_in6* in6 = (sockaddr_in6*)&addr;
            in6->sin6_family = AF_INET6;
            in6->sin6_addr = in6addr_loopback;
            in6->sin6_port = htons(at);
            len = sizeof(*in6);
        } else {
            sockaddr_in in4 = loopback(at);
            memcpy(&addr, &in4, sizeof(in4));
            len = sizeof(in4);
        }
        if (bind(listener, (sockaddr*)&addr, len) != 0 || listen(listener, 4) != 0 ||
                getsockname(listener, (sockaddr*)&addr, &len) != 0) {
            close_fd(listener);
            listener = NO_FD;
            return;
        }
        port = ntohs(family == AF_INET6 ?
                ((sockaddr_in6*)&addr)->sin6_port : ((sockaddr_in*)&addr)->sin_port);
    }

    ~FamilyServer() {
        if (listener != NO_FD) {
            close_fd(listener);
        }
    }

    // Whether a connection is waiting, which it takes and closes.
    bool took_one() {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        timeval timeout = { 1, 0 };
        if (select((int)listener + 1, &readable, nullptr, nullptr, &timeout) != 1) {
            return false;
        }
        fd_t peer = accept(listener, nullptr, nullptr);
        if (peer == NO_FD) {
            return false;
        }
        close_fd(peer);
        return true;
    }
};

}

// A server whose addresses don't share their TLS sessions resumes one only on
// the address that made it, and a lookup can give the addresses in another
// order each time, so that the next connection went where no session could be
// resumed. The socket now tries the address it last connected to first, while
// the host still has it.
TEST_CASE( "The plain TCP socket connects again to the address it last connected to" ) {
    std::vector<int> families = localhost_families();
    if (families.size() != 2 || families[0] == families[1]) {
        SKIP( "localhost isn't 127.0.0.1 and ::1 here, which this needs" );
    }
    // The second address, the only one listening at first, so that the first
    // connection goes there once the first address refuses.
    std::unique_ptr<FamilyServer> second(new FamilyServer(families[1], 0));
    if (second->listener == NO_FD) {
        SKIP( "no server could listen on localhost's second address here" );
    }
    const uint16_t port = second->port;
    tcp_socket_t sock;
    tcp_socket_init(&sock, 1, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;

    REQUIRE( ops->connect(&sock.base, "localhost", port) == EBISU_SOCK_OK );
    CHECK( second->took_one() );
    ops->close(&sock.base);

    std::unique_ptr<FamilyServer> first(new FamilyServer(families[0], port));
    if (first->listener == NO_FD) {
        SKIP( "no server could listen on localhost's first address at the same port" );
    }

    SECTION("the same address, though the lookup gives another first") {
        for (int i = 0; i < 3; ++i) {
            INFO( "connection " << i );
            REQUIRE( ops->connect(&sock.base, "localhost", port) == EBISU_SOCK_OK );
            CHECK( second->took_one() );
            CHECK_FALSE( first->took_one() );
            ops->close(&sock.base);
        }
    }

    SECTION("another address once that one refuses, and then that one") {
        second.reset();
        REQUIRE( ops->connect(&sock.base, "localhost", port) == EBISU_SOCK_OK );
        CHECK( first->took_one() );
        ops->close(&sock.base);

        second.reset(new FamilyServer(families[1], port));
        REQUIRE( second->listener != NO_FD );
        REQUIRE( ops->connect(&sock.base, "localhost", port) == EBISU_SOCK_OK );
        CHECK( first->took_one() );
        CHECK_FALSE( second->took_one() );
        ops->close(&sock.base);
    }

    SECTION("the lookup's first, for a host without that address") {
        REQUIRE( ops->connect(&sock.base, families[0] == AF_INET6 ? "::1" : "127.0.0.1",
                    port) == EBISU_SOCK_OK );
        CHECK( first->took_one() );
        CHECK_FALSE( second->took_one() );
        ops->close(&sock.base);
    }
}

