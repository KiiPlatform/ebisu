// platform's name servers on lwIP, ebisu_lwip_dns(): the servers lwIP's DNS
// client asks, and a query asked of one over a UDP socket of lwIP's, against a
// responder of the test's own on lwIP's loopback interface, then the DNS probe
// over them, and its socket over the lwIP socket. See README.md.

#include "lwip_test.h"

#include <ebisu_dns_probe.h>

#include <atomic>
#include <thread>

namespace {

// lwIP's DNS client's servers, set for a test, and none again after it, as the
// other tests have them: a name not in the local host list then fails at
// once.
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
#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
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

ebisu_dns_addr_t ipv4(const char* address, uint16_t port)
{
    ebisu_dns_addr_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.family = EBISU_DNS_IPV4;
    ip4_addr_t a;
    REQUIRE( ip4addr_aton(address, &a) == 1 );
    memcpy(addr.bytes, &a.addr, 4);
    addr.port = port;
    return addr;
}

// What an ask's is_answer was given: each datagram, and whether it took it.
struct Seen {
    std::vector<std::string> datagrams;
    static bool take_responses(const unsigned char* datagram, size_t length, void* userdata) {
        Seen* seen = static_cast<Seen*>(userdata);
        seen->datagrams.emplace_back((const char*)datagram, length);
        return length >= 4 && (datagram[2] & 0x80) != 0;
    }
};

const std::string QUERY("\x12\x34\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00", 12);

}

TEST_CASE( "lwIP's name servers are those its DNS client asks, in its order" ) {
    ebisu_dns_t* dns = ebisu_lwip_dns();
    REQUIRE( dns == ebisu_lwip_dns() );
    ebisu_dns_addr_t out[4];

    SECTION("none, as none is set") {
        CHECK( dns->ops->servers(dns, out, 4) == 0 );
    }
    SECTION("the first and the next, on port 53") {
        Servers servers("127.0.0.1", "10.255.0.2");
        REQUIRE( dns->ops->servers(dns, out, 4) == 2 );
        const unsigned char first[4] = { 127, 0, 0, 1 };
        const unsigned char second[4] = { 10, 255, 0, 2 };
        CHECK( out[0].family == EBISU_DNS_IPV4 );
        CHECK( memcmp(out[0].bytes, first, 4) == 0 );
        CHECK( out[0].port == 53 );
        CHECK( out[0].scope_id == 0 );
        CHECK( out[1].family == EBISU_DNS_IPV4 );
        CHECK( memcmp(out[1].bytes, second, 4) == 0 );
        CHECK( out[1].port == 53 );
        // As many as there is room for.
        CHECK( dns->ops->servers(dns, out, 1) == 1 );
        CHECK( dns->ops->servers(dns, out, 0) == 0 );
    }
    SECTION("none without the first, as the client asks the next only after it") {
        Servers servers(nullptr, "10.255.0.2");
        CHECK( dns->ops->servers(dns, out, 4) == 0 );
    }
#if LWIP_IPV6
    SECTION("an IPv6 one") {
        Servers servers("::1");
        REQUIRE( dns->ops->servers(dns, out, 4) == 1 );
        const unsigned char loopback6[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        CHECK( out[0].family == EBISU_DNS_IPV6 );
        CHECK( memcmp(out[0].bytes, loopback6, 16) == 0 );
        CHECK( out[0].port == 53 );
    }
#endif
}

TEST_CASE( "lwIP's name servers are asked over a UDP socket of lwIP's" ) {
    ebisu_dns_t* dns = ebisu_lwip_dns();
    char why[128] = "";
    Seen seen;

    SECTION("taking its answer") {
        Responder responder;
        ebisu_dns_addr_t server = ipv4("127.0.0.1", responder.port);
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 2000,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED );
        REQUIRE( seen.datagrams.size() == 1 );
        CHECK( seen.datagrams[0] == response_to(QUERY) );
        CHECK( responder.queries == 1 );
    }
    SECTION("past a datagram that isn't the answer") {
        Responder responder(0, Responder::NOISE_FIRST);
        ebisu_dns_addr_t server = ipv4("127.0.0.1", responder.port);
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 2000,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED );
        REQUIRE( seen.datagrams.size() == 2 );
        CHECK( seen.datagrams[0] == "noise" );
    }
    // The time given is for them all: one that isn't the answer, just before it
    // runs out, leaves only what is left of it.
    SECTION("for no longer than the time given, past a datagram that isn't the answer") {
        Responder responder(0, Responder::LATE_NOISE);
        ebisu_dns_addr_t server = ipv4("127.0.0.1", responder.port);
        auto start = std::chrono::steady_clock::now();
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 1000,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT );
        long ms = elapsed_ms(start);
        CHECK( ms >= 950 );
        CHECK( ms < 1600 );
        REQUIRE( seen.datagrams.size() == 1 );
        CHECK( seen.datagrams[0] == "noise" );
    }
    SECTION("without one from another port, which isn't the server's") {
        Responder responder(0, Responder::FROM_ELSEWHERE);
        ebisu_dns_addr_t server = ipv4("127.0.0.1", responder.port);
        auto start = std::chrono::steady_clock::now();
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 500,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT );
        CHECK( elapsed_ms(start) >= 450 );
        CHECK( seen.datagrams.empty() );
        CHECK( responder.queries == 1 );
    }
    SECTION("for the time given, of a server that never answers") {
        ebisu_dns_addr_t server = ipv4(SILENT, 53);
        auto start = std::chrono::steady_clock::now();
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 300,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT );
        long ms = elapsed_ms(start);
        CHECK( ms >= 250 );
        CHECK( ms < 3000 );
    }
    SECTION("for the time given, of one where nothing listens, which lwIP doesn't tell") {
        uint16_t closed = 0;
        {
            int s = Responder::bound(0);
            sockaddr_in addr;
            socklen_t len = sizeof(addr);
            REQUIRE( lwip_getsockname(s, (sockaddr*)&addr, &len) == 0 );
            closed = lwip_ntohs(addr.sin_port);
            lwip_close(s);
        }
        ebisu_dns_addr_t server = ipv4("127.0.0.1", closed);
        auto start = std::chrono::steady_clock::now();
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 300,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT );
        CHECK( elapsed_ms(start) >= 250 );
    }
#if !LWIP_IPV6
    SECTION("failing for a family lwIP is built without") {
        ebisu_dns_addr_t server;
        memset(&server, 0, sizeof(server));
        server.family = EBISU_DNS_IPV6;
        server.bytes[15] = 1;
        server.port = 53;
        CHECK( dns->ops->ask(dns, &server, (const unsigned char*)QUERY.data(), QUERY.size(), 300,
                    Seen::take_responses, &seen, why, sizeof(why)) == EBISU_DNS_ASKED_FAILED );
        CHECK( std::string(why) == "lwIP is built without IPv6" );
    }
#endif
    CHECK( dns->ops->no_answer_error(dns) == EAI_FAIL );
}

TEST_CASE( "The DNS probe asks lwIP's name servers" ) {
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE] = "";

    SECTION("one that answers") {
        Responder responder(53);
        Servers servers("127.0.0.1");
        CHECK( ebisu_dns_probe(ebisu_lwip_dns(), "host.example", 1000, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_ANSWERED );
        CHECK( std::string(detail) == "127.0.0.1:53 answered" );
        CHECK( responder.queries == 1 );
    }
    SECTION("the next, past one that doesn't") {
        Responder responder(53);
        Servers servers(SILENT, "127.0.0.1");
        CHECK( ebisu_dns_probe(ebisu_lwip_dns(), "host.example", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_ANSWERED );
        CHECK( std::string(detail) == "127.0.0.1:53 answered" );
    }
    SECTION("none that answers, in the time given each") {
        Servers servers(SILENT, SILENT_TOO);
        auto start = std::chrono::steady_clock::now();
        CHECK( ebisu_dns_probe(ebisu_lwip_dns(), "host.example", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_SILENT );
        long ms = elapsed_ms(start);
        CHECK( ms >= 550 );
        CHECK( ms < 3000 );
        CHECK( std::string(detail) == "10.255.0.3:53 didn't answer in time" );
    }
    SECTION("none set") {
        CHECK( ebisu_dns_probe(ebisu_lwip_dns(), "host.example", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_SILENT );
        CHECK( std::string(detail) == "no server to ask" );
    }
}

// What the probe is for on lwIP: its DNS client gives each server about 7
// seconds, 14 for two that don't answer, and the probing socket a second.
TEST_CASE( "The probing socket fails a connect over lwIP at once when no name server answers" ) {
    ebisu_lwip_tcp_socket_t tcp;
    ebisu_lwip_tcp_socket_init(&tcp, 5, 5);
    ebisu_dns_probe_socket_t sock;
    ebisu_dns_probe_socket_init(&sock, &tcp.base, ebisu_lwip_dns(), 500);
    const ebisu_socket_ops_t* ops = sock.base.ops;
    long detail = 0;

    SECTION("failing it when none answers") {
        Servers servers(SILENT, SILENT_TOO);
        auto start = std::chrono::steady_clock::now();
        REQUIRE( ops->connect(&sock.base, "missing.test", 1883) == EBISU_SOCK_FAIL );
        long ms = elapsed_ms(start);
        CHECK( ms >= 900 );
        CHECK( ms < 3000 );
        CHECK( ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS );
        CHECK( detail == EAI_FAIL );
    }
    SECTION("connecting when one does") {
        Responder responder(53);
        Servers servers("127.0.0.1");
        Server server;
        REQUIRE( ops->connect(&sock.base, "server.test", server.port) == EBISU_SOCK_OK );
        server.accept_one();
        size_t sent = 0;
        REQUIRE( ops->send(&sock.base, "probed", 6, &sent) == EBISU_SOCK_OK );
        CHECK( server.receive(6) == "probed" );
        ops->close(&sock.base);
    }
}
