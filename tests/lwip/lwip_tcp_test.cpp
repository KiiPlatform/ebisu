// platform's socket of plain TCP over lwIP's sockets, against servers of the
// test's own, on lwIP's sockets too, on its loopback interface: lwIP runs in
// this process, on a port to POSIX threads, and reaches no other host. An
// interface that drops what it is given stands for an address that doesn't
// answer. See README.md.

#include "lwip_test.h"

namespace {

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

// The tests' lwipopts.h leaves MEMP_NUM_NETDB at lwIP's default: one lookup's
// result at a time, which lwip_getaddrinfo() takes and lwip_freeaddrinfo()
// gives back.
#if MEMP_NUM_NETDB != 1
#error "lwipopts.h has room for more than one lookup's result"
#endif

TEST_CASE( "The lwIP socket's lookup leaves lwIP's one result to other tasks" ) {
    Server server;
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 5, 5);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    long detail = -1;

    SECTION("while it connects") {
        // Another task's socket, connecting to an address that doesn't
        // answer, for its connect timeout.
        ebisu_lwip_tcp_socket_t other;
        ebisu_lwip_tcp_socket_init(&other, 5, 5);
        ebisu_lwip_tcp_socket_set_connect_timeout(&other, 2);
        std::atomic<int> other_code(-1);
        std::thread connecting([&] {
            other_code = other.base.ops->connect(&other.base, SILENT, 1883);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        auto start = std::chrono::steady_clock::now();
        CHECK( ops->connect(&sock.base, "server.test", server.port) == EBISU_SOCK_OK );
        CHECK( elapsed_ms(start) < 1000 );
        connecting.join();
        CHECK( other_code == EBISU_SOCK_FAIL );
        server.accept_one();
    }

    SECTION("and waits for it while another task's lookup holds it") {
        // Catch's assertions from the test's own thread alone.
        std::atomic<bool> held(false);
        std::atomic<int> looked_up(-1);
        std::thread holding([&] {
            struct addrinfo* results = nullptr;
            looked_up = lwip_getaddrinfo("server.test", "80", nullptr, &results);
            held = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            if (results != nullptr) {
                lwip_freeaddrinfo(results);
            }
        });
        while (!held) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto start = std::chrono::steady_clock::now();
        CHECK( ops->connect(&sock.base, "server.test", server.port) == EBISU_SOCK_OK );
        CHECK( elapsed_ms(start) >= 100 );
        holding.join();
        CHECK( looked_up == 0 );
        server.accept_one();
    }

    SECTION("for half a second at the most, and then fails with EAI_MEMORY") {
        // Catch's assertions from the test's own thread alone.
        std::atomic<bool> held(false);
        std::atomic<int> looked_up(-1);
        std::thread holding([&] {
            struct addrinfo* results = nullptr;
            looked_up = lwip_getaddrinfo("server.test", "80", nullptr, &results);
            held = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            if (results != nullptr) {
                lwip_freeaddrinfo(results);
            }
        });
        while (!held) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto start = std::chrono::steady_clock::now();
        CHECK( ops->connect(&sock.base, "server.test", server.port) == EBISU_SOCK_FAIL );
        long ms = elapsed_ms(start);
        CHECK( ms >= 400 );
        CHECK( ms < 2500 );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS );
        CHECK( detail == EAI_MEMORY );
        holding.join();
        CHECK( looked_up == 0 );
    }
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
        // As a name server says, which lwIP 1.4's DNS client needs.
        Responder responder(53);
        Servers servers("127.0.0.1");
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
        Responder responder(53);
        Servers servers("127.0.0.1");
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

// A receive timeout as lwIP is built to take it: an int of milliseconds in
// lwIP 1.4, and with LWIP_SO_SNDRCVTIMEO_NONSTANDARD, a struct timeval
// otherwise.
TEST_CASE( "The lwIP socket sets its timeouts as lwIP takes them" ) {
    Server server;
    ebisu_lwip_tcp_socket_t sock;
    ebisu_lwip_tcp_socket_init(&sock, 7, 3);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    REQUIRE( ops->connect(&sock.base, "127.0.0.1", server.port) == EBISU_SOCK_OK );
    server.accept_one();
#if TEST_TIMEOUT_MS
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
