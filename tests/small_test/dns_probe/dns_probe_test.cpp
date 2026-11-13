#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <ebisu_dns_probe.h>
#include "ebisu_dns_probe_impl.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::string query_for(const char* host, uint16_t id) {
    unsigned char out[_EBISU_DNS_QUERY_MAX];
    size_t length = _ebisu_dns_probe_build_query(host, id, out, sizeof(out));
    return std::string((const char*)out, length);
}

int64_t elapsed_ms(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - since).count();
}

// A name server on the loopback, on a port the system picks, that answers each
// query as its mode says, from a thread of its own.
struct FakeServer {
    enum Mode {
        // An answer that the name doesn't exist.
        ANSWER,
        // Datagrams that aren't the answer, then the answer.
        OTHERS_FIRST,
        // Datagrams that aren't the answer, and another 700 ms later.
        OTHERS_ONLY,
        // Nothing.
        SILENT
    };

    int sock;
    uint16_t port;
    bool listening;
    Mode mode;
    std::atomic<bool> stopping;
    std::mutex mutex;
    std::vector<std::string> queries;
    std::thread thread;

    explicit FakeServer(Mode mode, int family = AF_INET)
        : sock(-1), port(0), listening(false), mode(mode), stopping(false) {
        sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        if (family == AF_INET) {
            sockaddr_in* in = (sockaddr_in*)&addr;
            in->sin_family = AF_INET;
            in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            start(addr, sizeof(*in));
        } else {
            sockaddr_in6* in6 = (sockaddr_in6*)&addr;
            in6->sin6_family = AF_INET6;
            in6->sin6_addr = in6addr_loopback;
            start(addr, sizeof(*in6));
        }
    }

    // On an IPv6 address of the system's, with its scope.
    FakeServer(Mode mode, const sockaddr_in6& at)
        : sock(-1), port(0), listening(false), mode(mode), stopping(false) {
        sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        memcpy(&addr, &at, sizeof(at));
        ((sockaddr_in6*)&addr)->sin6_port = 0;
        start(addr, sizeof(at));
    }

    void start(sockaddr_storage addr, socklen_t length) {
        int family = addr.ss_family;
        sock = socket(family, SOCK_DGRAM, 0);
        if (sock < 0 || bind(sock, (sockaddr*)&addr, length) != 0 ||
                getsockname(sock, (sockaddr*)&addr, &length) != 0) {
            return;
        }
        port = ntohs(family == AF_INET ? ((sockaddr_in*)&addr)->sin_port
                                       : ((sockaddr_in6*)&addr)->sin6_port);
        listening = true;
        thread = std::thread([this] { serve(); });
    }

    ~FakeServer() {
        stopping = true;
        if (thread.joinable()) {
            thread.join();
        }
        if (sock >= 0) {
            close(sock);
        }
    }

    std::vector<std::string> received() {
        std::lock_guard<std::mutex> lock(mutex);
        return queries;
    }

    void serve() {
        // Where OTHERS_ONLY sends its last, and when.
        std::string other;
        sockaddr_storage peer;
        socklen_t peer_length = 0;
        auto next = std::chrono::steady_clock::now();
        while (!stopping) {
            if (peer_length != 0 && std::chrono::steady_clock::now() >= next) {
                send_to(other, peer, peer_length);
                peer_length = 0;
            }
            pollfd p;
            p.fd = sock;
            p.events = POLLIN;
            p.revents = 0;
            if (poll(&p, 1, 10) <= 0) {
                continue;
            }
            unsigned char datagram[512];
            sockaddr_storage from;
            socklen_t from_length = sizeof(from);
            ssize_t n = recvfrom(sock, datagram, sizeof(datagram), 0, (sockaddr*)&from,
                    &from_length);
            if (n <= 0) {
                continue;
            }
            std::string query((const char*)datagram, (size_t)n);
            {
                std::lock_guard<std::mutex> lock(mutex);
                queries.push_back(query);
            }
            if (mode == SILENT || query.size() < 12) {
                continue;
            }
            // An answer, recursion asked as the query did and available, and
            // NXDOMAIN.
            std::string answer = query;
            answer[2] = (char)(0x80 | (query[2] & 0x01));
            answer[3] = (char)0x83;
            if (mode == OTHERS_FIRST || mode == OTHERS_ONLY) {
                std::string other_id = answer;
                other_id[1] = (char)(other_id[1] ^ 1);
                send_to(other_id, from, from_length);
                // The query itself: no answer, without QR.
                send_to(query, from, from_length);
                send_to(answer.substr(0, 11), from, from_length);
            }
            if (mode != OTHERS_ONLY) {
                send_to(answer, from, from_length);
            } else {
                other = answer;
                other[1] = (char)(other[1] ^ 1);
                peer = from;
                peer_length = from_length;
                next = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
            }
        }
    }

    void send_to(const std::string& datagram, const sockaddr_storage& to, socklen_t length) {
        sendto(sock, datagram.data(), datagram.size(), 0, (const sockaddr*)&to, length);
    }
};

// A port of the loopback nothing listens on, as far as anything can tell.
uint16_t unused_port() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t length = sizeof(addr);
    uint16_t port = 0;
    if (bind(sock, (sockaddr*)&addr, length) == 0 &&
            getsockname(sock, (sockaddr*)&addr, &length) == 0) {
        port = ntohs(addr.sin_port);
    }
    close(sock);
    return port;
}

std::string at(uint16_t port) {
    return "127.0.0.1:" + std::to_string(port);
}

// A server as text: its address, with its scope's number if it has one, and
// its port.
std::string text_of(const _ebisu_dns_addr_t& server) {
    char address[INET6_ADDRSTRLEN];
    inet_ntop(server.family, server.bytes, address, sizeof(address));
    std::string text = address;
    if (server.scope_id != 0) {
        text += "%" + std::to_string(server.scope_id);
    }
    return text + " " + std::to_string(server.port);
}

// The servers of a resolv.conf, as text, read as musl does or, with as_glibc,
// as glibc does.
std::vector<std::string> servers_of(const std::string& text, bool as_glibc = false) {
    _ebisu_dns_addr_t servers[_EBISU_DNS_SERVERS_MAX];
    size_t count = _ebisu_dns_probe_parse_resolv_conf(text.data(), text.size(), servers,
            _EBISU_DNS_SERVERS_MAX, as_glibc);
    std::vector<std::string> out;
    for (size_t i = 0; i < count; ++i) {
        out.push_back(text_of(servers[i]));
    }
    return out;
}

// An interface of this system's: its name and its number, or an empty name.
std::pair<std::string, uint32_t> an_interface() {
    std::pair<std::string, uint32_t> found("", 0);
    struct if_nameindex* all = if_nameindex();
    if (all != nullptr) {
        if (all[0].if_index != 0) {
            found = std::make_pair(std::string(all[0].if_name), (uint32_t)all[0].if_index);
        }
        if_freenameindex(all);
    }
    return found;
}

// A link-local IPv6 address of this system's, with its interface's number and
// name, that a socket binds to, if there is one.
bool a_link_local(sockaddr_in6* out, std::string* out_name) {
    ifaddrs* all = nullptr;
    if (getifaddrs(&all) != 0) {
        return false;
    }
    bool found = false;
    for (ifaddrs* a = all; a != nullptr && !found; a = a->ifa_next) {
        if (a->ifa_addr == nullptr || a->ifa_addr->sa_family != AF_INET6 ||
                (a->ifa_flags & IFF_UP) == 0) {
            continue;
        }
        sockaddr_in6 at;
        memcpy(&at, a->ifa_addr, sizeof(at));
        if (!IN6_IS_ADDR_LINKLOCAL(&at.sin6_addr) || at.sin6_scope_id == 0) {
            continue;
        }
        at.sin6_port = 0;
        int sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock >= 0 && bind(sock, (sockaddr*)&at, sizeof(at)) == 0) {
            *out = at;
            *out_name = a->ifa_name;
            found = true;
        }
        if (sock >= 0) {
            close(sock);
        }
    }
    freeifaddrs(all);
    return found;
}

// A file of the test's own, removed when it goes.
struct TempFile {
    std::string path;

    explicit TempFile(const std::string& content) {
        char name[] = "dns_probe_test_XXXXXX";
        int fd = mkstemp(name);
        path = name;
        if (fd >= 0) {
            if (write(fd, content.data(), content.size()) != (ssize_t)content.size()) {
                path.clear();
            }
            close(fd);
        }
    }

    ~TempFile() {
        if (!path.empty()) {
            unlink(path.c_str());
        }
    }
};

// The servers of the file at path, read as musl does or, with as_glibc, as glibc
// does.
std::vector<std::string> read_servers(const char* path, bool as_glibc) {
    _ebisu_dns_addr_t servers[_EBISU_DNS_SERVERS_MAX];
    size_t count = _ebisu_dns_probe_read_servers(path, servers, _EBISU_DNS_SERVERS_MAX,
            as_glibc);
    std::vector<std::string> out;
    for (size_t i = 0; i < count; ++i) {
        out.push_back(text_of(servers[i]));
    }
    return out;
}

// The socket the probe's is put in front of, which counts what it is asked.
struct Inner;
struct InnerSocket {
    ebisu_socket_t base;
    Inner* inner;
};

struct Inner {
    InnerSocket sock;
    ebisu_socket_ops_t ops;
    std::vector<ebisu_sock_code_t> connect_codes;
    int connects = 0, sends = 0, recvs = 0, closes = 0, timed_out_asked = 0, waits = 0,
        errors_asked = 0;
    std::string host;
    uint16_t port = 0;
    uint32_t wait_ms = 0;

    static Inner* of(ebisu_socket_t* self) { return ((InnerSocket*)self)->inner; }

    static ebisu_sock_code_t connect(ebisu_socket_t* self, const char* host, uint16_t port) {
        Inner* in = of(self);
        in->host = host;
        in->port = port;
        size_t i = (size_t)in->connects++;
        return i < in->connect_codes.size() ? in->connect_codes[i] : EBISU_SOCK_OK;
    }
    static ebisu_sock_code_t send(ebisu_socket_t* self, const char*, size_t length,
            size_t* out_sent_length) {
        ++of(self)->sends;
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    }
    static ebisu_sock_code_t recv(ebisu_socket_t* self, char* buffer, size_t,
            size_t* out_actual_length) {
        ++of(self)->recvs;
        buffer[0] = 'x';
        *out_actual_length = 1;
        return EBISU_SOCK_OK;
    }
    static ebisu_sock_code_t close(ebisu_socket_t* self) {
        ++of(self)->closes;
        return EBISU_SOCK_OK;
    }
    static bool recv_timed_out(ebisu_socket_t* self) {
        ++of(self)->timed_out_asked;
        return true;
    }
    static ebisu_sock_error_t last_error(ebisu_socket_t* self, long* out_detail) {
        ++of(self)->errors_asked;
        *out_detail = 111;
        return EBISU_SOCK_ERROR_CONNECT;
    }
    static bool recv_wait(ebisu_socket_t* self, uint32_t timeout_ms) {
        ++of(self)->waits;
        of(self)->wait_ms = timeout_ms;
        return true;
    }

    Inner(bool with_timed_out, bool with_wait, bool with_last_error = true) {
        ops.connect = connect;
        ops.send = send;
        ops.recv = recv;
        ops.close = close;
        ops.recv_timed_out = with_timed_out ? recv_timed_out : nullptr;
        ops.last_error = with_last_error ? last_error : nullptr;
        ops.recv_wait = with_wait ? recv_wait : nullptr;
        sock.base.ops = &ops;
        sock.inner = this;
    }
};

// A logger that keeps what it is given.
struct Lines {
    ebisu_logger_t base;
    std::vector<std::pair<ebisu_log_level_t, std::string> >* lines;
};

void keep_line(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args) {
    char line[1024];
    vsnprintf(line, sizeof(line), fmt, args);
    ((Lines*)self)->lines->push_back(std::make_pair(level, std::string(line)));
}

const ebisu_logger_ops_t keep_line_ops = { keep_line };

} // namespace

TEST_CASE("A probe's query asks once, recursively, for the name's IPv4 addresses") {
    static const unsigned char expected[] = {
        0x12, 0x34, // the identifier
        0x01, 0x00, // a query, recursion desired
        0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // one question
        7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0,
        0x00, 0x01, // A
        0x00, 0x01 // IN
    };
    const std::string bytes((const char*)expected, sizeof(expected));
    CHECK(query_for("example.com", 0x1234) == bytes);

    SECTION("the same with a trailing dot") {
        CHECK(query_for("example.com.", 0x1234) == bytes);
    }
    SECTION("of one label") {
        CHECK(query_for("a", 0xfffe) ==
                std::string("\xff\xfe\x01\x00\x00\x01\x00\x00\x00\x00\x00\x00\x01" "a"
                    "\x00\x00\x01\x00\x01", 19));
    }
    SECTION("of the longest name, of the longest labels") {
        std::string name = std::string(63, 'a') + "." + std::string(63, 'b') + "." +
            std::string(63, 'c') + "." + std::string(61, 'd');
        REQUIRE(name.size() == 253);
        std::string query = query_for(name.c_str(), 1);
        REQUIRE(query.size() == (size_t)_EBISU_DNS_QUERY_MAX);
        CHECK(query.substr(12, 2) == std::string("\x3f" "a"));
        CHECK((unsigned char)query[12 + 64] == 63);
        CHECK((unsigned char)query[12 + 3 * 64] == 61);
        CHECK(query.substr(query.size() - 5) == std::string("\x00\x00\x01\x00\x01", 5));
    }
}

TEST_CASE("A probe builds no query for a name no query can carry") {
    unsigned char out[_EBISU_DNS_QUERY_MAX];
    const std::string label_too_long = std::string(64, 'a') + ".com";
    const std::string name_too_long = std::string(63, 'a') + "." + std::string(63, 'b') +
        "." + std::string(63, 'c') + "." + std::string(62, 'd');
    const char* name = nullptr;
    SECTION("no name") {
    }
    SECTION("an empty one") {
        name = "";
    }
    SECTION("the root alone") {
        name = ".";
    }
    SECTION("an empty label") {
        name = "a..com";
    }
    SECTION("an empty first label") {
        name = ".com";
    }
    SECTION("two trailing dots") {
        name = "a.com..";
    }
    SECTION("a label too long") {
        name = label_too_long.c_str();
    }
    SECTION("a name too long") {
        REQUIRE(name_too_long.size() == 254);
        name = name_too_long.c_str();
    }
    CHECK(_ebisu_dns_probe_build_query(name, 1, out, sizeof(out)) == 0);
    CHECK_FALSE(_ebisu_dns_probe_needs_lookup(name));
}

TEST_CASE("A probe builds no query in a buffer too small for it") {
    unsigned char out[_EBISU_DNS_QUERY_MAX];
    const size_t needed = 12 + 13 + 4;
    CHECK(_ebisu_dns_probe_build_query("example.com", 1, out, needed - 1) == 0);
    CHECK(_ebisu_dns_probe_build_query("example.com", 1, out, needed) == needed);
}

TEST_CASE("A probe takes any answer to its query, and nothing else") {
    std::string answer = query_for("example.com", 0xabcd);
    answer[2] = (char)0x81;
    answer[3] = (char)0x80;
    const unsigned char* bytes = (const unsigned char*)answer.data();
    CHECK(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xabcd));
    // Only the header is read.
    CHECK(_ebisu_dns_probe_is_answer(bytes, 12, 0xabcd));

    SECTION("that the name doesn't exist, or that the server failed, too") {
        answer[3] = (char)0x83;
        CHECK(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xabcd));
        answer[3] = (char)0x82;
        CHECK(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xabcd));
    }
    SECTION("but not a shorter header") {
        CHECK_FALSE(_ebisu_dns_probe_is_answer(bytes, 11, 0xabcd));
        CHECK_FALSE(_ebisu_dns_probe_is_answer(nullptr, 0, 0xabcd));
    }
    SECTION("nor an answer to another query") {
        CHECK_FALSE(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xabcc));
        CHECK_FALSE(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xbbcd));
    }
    SECTION("nor a query") {
        answer[2] = (char)0x01;
        CHECK_FALSE(_ebisu_dns_probe_is_answer(bytes, answer.size(), 0xabcd));
    }
}

TEST_CASE("A probe tells which hosts need a name server to be looked up") {
    for (const char* host : { "example.com", "api.example.com.", "a", "localhost.example.com",
            "notlocalhost", "localhost-1", "1.2.3.4.", "1.2.3.4.5", "256.0.0.1", "1.2.3.256",
            "1.2.65536", "1.16777216", "4294967296", "08", "0x", "0xg", "1.2.3.4%eth0",
            "+1", " 1", "1 ", "a:b", "fe80::g" }) {
        INFO(host);
        CHECK(_ebisu_dns_probe_needs_lookup(host));
    }
    for (const char* host : { "127.0.0.1", "0.0.0.0", "255.255.255.255", "127.1", "10.1.2",
            "0x7f.1", "0X7F.0.0.01", "017700000001", "2130706433", "4294967295", "0",
            "::1", "::", "2001:db8::1", "::ffff:127.0.0.1", "fe80::1%eth0", "fe80::1%",
            "localhost", "LOCALHOST", "localhost.", "a.localhost", "a.b.LocalHost." }) {
        INFO(host);
        CHECK_FALSE(_ebisu_dns_probe_needs_lookup(host));
    }
}

TEST_CASE("A probe reads IPv4 addresses as inet_aton() does") {
    for (const char* s : { "127.0.0.1", "127.1", "10.1.2", "2130706433", "0x7f000001",
            "0177.0.0.1", "0x7f.0x0.0.1", "00", "0x0", "0xFF.0377.255.0", "255.255.255.255",
            "4294967295", "256.1.1.1", "1.256.1.1", "1.2.65536", "1.2.65535", "1.16777215",
            "1.16777216", "08", "09.1", "0x", "0xg", "1.", ".1", "1..1", "1.2.3.4.",
            "1.2.3.4.5", "a", "", "1a", "-1", "+1", "1.+2", "1. 2" }) {
        INFO(s);
        unsigned char ours[4];
        in_addr theirs;
        bool parsed = _ebisu_dns_probe_parse_ipv4(s, strlen(s), ours);
        REQUIRE(parsed == (inet_aton(s, &theirs) != 0));
        if (parsed) {
            CHECK(memcmp(ours, &theirs.s_addr, 4) == 0);
        }
    }
    unsigned char out[4];
    // A number past 32 bits, which glibc refuses and 32-bit musl takes for
    // 0xffffffff.
    for (const char* s : { "4294967296", "0x100000000", "1.99999999999999999999",
            "99999999999999999999" }) {
        INFO(s);
        CHECK_FALSE(_ebisu_dns_probe_parse_ipv4(s, strlen(s), out));
    }
    // Of the length given, not up to a NUL.
    CHECK(_ebisu_dns_probe_parse_ipv4("10.0.0.1x", 8, out));
    CHECK_FALSE(_ebisu_dns_probe_parse_ipv4("10.0.0.1", 9, out));
}

TEST_CASE("A probe asks the servers of resolv.conf, as the C libraries read it") {
    CHECK(servers_of("") == std::vector<std::string>());
    CHECK(servers_of("nameserver 10.0.0.1") == std::vector<std::string>{ "10.0.0.1 53" });
    CHECK(servers_of(
            "# Generated\n"
            "search example.com\n"
            "options timeout:2 attempts:3\n"
            "nameserver 10.0.0.1\n"
            "nameserver\t::1\n"
            "nameserver 10.0.0.2\n"
            "nameserver 10.0.0.3\n") ==
            std::vector<std::string>{ "10.0.0.1 53", "::1 53", "10.0.0.2 53" });

    SECTION("an address ended by a blank, a comment after it") {
        for (bool as_glibc : { false, true }) {
            INFO("as_glibc " << as_glibc);
            CHECK(servers_of(
                    "nameserver 10.0.0.1 # the router\n"
                    "nameserver\t10.0.0.2\t;and\n"
                    "nameserver \t 10.0.0.3   \n", as_glibc) ==
                    std::vector<std::string>{ "10.0.0.1 53", "10.0.0.2 53", "10.0.0.3 53" });
        }
    }
    SECTION("an address a comment touches skipped, as by the C libraries") {
        for (bool as_glibc : { false, true }) {
            INFO("as_glibc " << as_glibc);
            CHECK(servers_of(
                    "nameserver 10.0.0.1#the router\n"
                    "nameserver 10.0.0.2;and\n"
                    "nameserver ::3#\n"
                    "nameserver 10.0.0.4\n", as_glibc) ==
                    std::vector<std::string>{ "10.0.0.4 53" });
        }
    }
    SECTION("a line split at what isspace() takes, as musl does") {
        CHECK(servers_of(
                "nameserver 10.0.0.1\r\n"
                "nameserver\v10.0.0.2\n"
                "nameserver \f10.0.0.3\v\n") ==
                std::vector<std::string>{ "10.0.0.1 53", "10.0.0.2 53", "10.0.0.3 53" });
        CHECK(servers_of("nameserver 10.0.0.4\rx\n") ==
                std::vector<std::string>{ "10.0.0.4 53" });
        CHECK(servers_of("nameserver ::5\r\n") == std::vector<std::string>{ "::5 53" });
    }
    SECTION("a line split at spaces and tabs only, as glibc does") {
        CHECK(servers_of(
                "nameserver 10.0.0.1\r\n"
                "nameserver\v10.0.0.2\n"
                "nameserver \f10.0.0.3\n"
                "nameserver 10.0.0.4\rx\n"
                "nameserver ::5\r\n"
                "nameserver 10.0.0.6 \r\n", true) ==
                std::vector<std::string>{ "10.0.0.6 53" });
        // What glibc doesn't split at is part of the scope, which it can't
        // read, and the server is asked without it.
        const auto iface = an_interface();
        REQUIRE_FALSE(iface.first.empty());
        const std::string line = "nameserver fe80::7%" + iface.first + "\r\n";
        CHECK(servers_of(line, true) == std::vector<std::string>{ "fe80::7 53" });
        CHECK(servers_of(line) == std::vector<std::string>{
                "fe80::7%" + std::to_string(iface.second) + " 53" });
    }
    SECTION("in a form inet_aton() takes, as glibc does") {
        CHECK(servers_of("nameserver 127.1\n") == std::vector<std::string>{ "127.0.0.1 53" });
    }
    SECTION("a server repeated taking a place again, as in the C libraries") {
        CHECK(servers_of(
                "nameserver 10.0.0.1\n"
                "nameserver 10.0.0.1\n"
                "nameserver 10.0.0.1\n"
                "nameserver 10.0.0.2\n") ==
                std::vector<std::string>{ "10.0.0.1 53", "10.0.0.1 53", "10.0.0.1 53" });
    }
    SECTION("an IPv6 address with its scope, as the C libraries read it") {
        const auto iface = an_interface();
        REQUIRE_FALSE(iface.first.empty());
        const std::string index = std::to_string(iface.second);
        const std::string text =
                "nameserver fe80::1%no-such-if0\n"
                "nameserver 2001:db8::2%" + iface.first + "\n"
                "nameserver fe80::3%" + iface.first + "\n"
                "nameserver 2001:db8::4%7\n"
                "nameserver 10.0.0.5\n";
        // musl skips a line whose scope it can't read.
        CHECK(servers_of(text) == std::vector<std::string>{
                "fe80::3%" + index + " 53", "2001:db8::4%7 53", "10.0.0.5 53" });
        // glibc asks the server without it.
        CHECK(servers_of(text, true) == std::vector<std::string>{
                "fe80::1 53", "2001:db8::2 53", "fe80::3%" + index + " 53" });
    }
    SECTION("skipping what isn't an address") {
        CHECK(servers_of(
                "nameserver 10.0.0.1x\n"
                "nameserver dns.example.com\n"
                "nameserver\n"
                "nameserver \n"
                "nameserver 10.0.0.9") == std::vector<std::string>{ "10.0.0.9 53" });
    }
    SECTION("skipping what isn't a nameserver line") {
        CHECK(servers_of(
                " nameserver 10.0.0.1\n"
                "#nameserver 10.0.0.2\n"
                ";nameserver 10.0.0.3\n"
                "nameservers 10.0.0.4\n"
                "Nameserver 10.0.0.5\n"
                "nameserver10.0.0.6\n") == std::vector<std::string>());
    }
    SECTION("as many as the C libraries take") {
        std::string text;
        for (int i = 1; i <= 5; ++i) {
            text += "nameserver 10.0.0." + std::to_string(i) + "\n";
        }
        CHECK(servers_of(text).size() == _EBISU_DNS_SERVERS_MAX);
    }
}

TEST_CASE("A probe asks the system's servers, 127.0.0.1 if there are none") {
    TempFile none("# no servers\nsearch example.com\n");
    REQUIRE_FALSE(none.path.empty());
    // What is after the 255 bytes a long line's first read holds isn't a line
    // of its own, and the last line needs no newline.
    TempFile some("#" + std::string(254, 'x') + "nameserver 10.0.0.8\n"
            "# " + std::string(1000, 'x') + " nameserver 10.0.0.9\n"
            "nameserver ::1\n"
            "nameserver 10.0.0.2");
    REQUIRE_FALSE(some.path.empty());
    for (bool as_glibc : { false, true }) {
        INFO("as_glibc " << as_glibc);
        CHECK(read_servers("/nonexistent/resolv.conf", as_glibc) ==
                std::vector<std::string>{ "127.0.0.1 53" });
        CHECK(read_servers(none.path.c_str(), as_glibc) ==
                std::vector<std::string>{ "127.0.0.1 53" });
        CHECK(read_servers(some.path.c_str(), as_glibc) ==
                std::vector<std::string>{ "::1 53", "10.0.0.2 53" });
        // Whatever this system has, there is one.
        CHECK_FALSE(read_servers("/etc/resolv.conf", as_glibc).empty());
    }

    // A scope read as glibc does, and as musl does.
    TempFile scoped("nameserver fe80::1%no-such-if0\nnameserver 10.0.0.1\n");
    REQUIRE_FALSE(scoped.path.empty());
    CHECK(read_servers(scoped.path.c_str(), true) ==
            std::vector<std::string>{ "fe80::1 53", "10.0.0.1 53" });
    CHECK(read_servers(scoped.path.c_str(), false) ==
            std::vector<std::string>{ "10.0.0.1 53" });
}

TEST_CASE("A probe reads a long line of resolv.conf as the C libraries do") {
    // 254 bytes, which musl's buffer of 256 holds with a newline, and 255,
    // which it doesn't.
    const std::string fits = "nameserver 10.0.0.1" + std::string(235, ' ');
    const std::string too_long = "nameserver 10.0.0.2" + std::string(236, ' ');
    REQUIRE(fits.size() == 254);
    REQUIRE(too_long.size() == 255);
    // An address after more blanks than that.
    const std::string far = "nameserver" + std::string(300, '\t') + "10.0.0.3";

    TempFile lines(fits + "\n" + too_long + "\n" + far + "\n");
    REQUIRE_FALSE(lines.path.empty());
    // musl skips a line its buffer doesn't hold, and glibc reads it whole.
    CHECK(read_servers(lines.path.c_str(), false) == std::vector<std::string>{ "10.0.0.1 53" });
    CHECK(read_servers(lines.path.c_str(), true) ==
            std::vector<std::string>{ "10.0.0.1 53", "10.0.0.2 53", "10.0.0.3 53" });

    // The last line, without a newline, as any other.
    TempFile last_fits("nameserver 10.0.0.4\n" + fits);
    TempFile last_too_long("nameserver 10.0.0.4\n" + too_long);
    REQUIRE_FALSE(last_fits.path.empty());
    REQUIRE_FALSE(last_too_long.path.empty());
    CHECK(read_servers(last_fits.path.c_str(), false) ==
            std::vector<std::string>{ "10.0.0.4 53", "10.0.0.1 53" });
    CHECK(read_servers(last_too_long.path.c_str(), false) ==
            std::vector<std::string>{ "10.0.0.4 53" });
    CHECK(read_servers(last_too_long.path.c_str(), true) ==
            std::vector<std::string>{ "10.0.0.4 53", "10.0.0.2 53" });
}

TEST_CASE("A probe reads resolv.conf as this system's C library does") {
    // confstr() has a glibc version only for glibc.
    char version[64] = "";
#ifdef _CS_GNU_LIBC_VERSION
    confstr(_CS_GNU_LIBC_VERSION, version, sizeof(version));
#endif
    INFO("version " << version);
    CHECK(_EBISU_DNS_READ_AS_GLIBC == (strncmp(version, "glibc ", 6) == 0));
}

TEST_CASE("A probe reads an IPv6 address's scope as the C libraries do") {
    const auto iface = an_interface();
    REQUIRE_FALSE(iface.first.empty());
    const std::string name = iface.first;
    // Its scope, if s is an IPv6 address.
    auto scope_of = [](const std::string& s, bool keep_bad_scope, uint32_t* out) {
        _ebisu_dns_addr_t addr;
        addr.scope_id = 99;
        if (!_ebisu_dns_probe_parse_address(s.data(), s.size(), 53, keep_bad_scope, &addr)) {
            return false;
        }
        *out = addr.scope_id;
        return addr.family == AF_INET6 && addr.port == 53;
    };
    // Read as musl does, then as glibc does.
    const bool readings[] = { false, true };

    SECTION("an interface's number, for any address") {
        for (bool keep_bad_scope : readings) {
            INFO("keep_bad_scope " << keep_bad_scope);
            uint32_t scope = 99;
            CHECK(scope_of("fe80::1%3", keep_bad_scope, &scope));
            CHECK(scope == 3);
            CHECK(scope_of("2001:db8::1%5", keep_bad_scope, &scope));
            CHECK(scope == 5);
            CHECK(scope_of("fe80::1%007", keep_bad_scope, &scope));
            CHECK(scope == 7);
            CHECK(scope_of("fe80::1%4294967295", keep_bad_scope, &scope));
            CHECK(scope == 4294967295u);
            CHECK(scope_of("fe80::1%0", keep_bad_scope, &scope));
            CHECK(scope == 0);
            scope = 99;
            CHECK(scope_of("fe80::1", keep_bad_scope, &scope));
            CHECK(scope == 0);
        }
    }
    SECTION("an interface's name, for a link-local address") {
        for (bool keep_bad_scope : readings) {
            for (const char* address : { "fe80::1", "febf::1", "ff02::1", "ff12::1" }) {
                INFO("keep_bad_scope " << keep_bad_scope << ", " << address);
                uint32_t scope = 99;
                CHECK(scope_of(std::string(address) + "%" + name, keep_bad_scope, &scope));
                CHECK(scope == iface.second);
            }
        }
    }
    SECTION("another scope: refused, as musl does, or left out, as glibc does") {
        const std::vector<std::string> others = { "fe80::1%", "fe80::1%4294967296",
                "fe80::1%99999999999999999999", "fe80::1%+1", "fe80::1%-1",
                "fe80::1%no-such-if0", "fe80::1%" + name + "%" + name,
                "fe80::1%" + std::string(IF_NAMESIZE, 'x'), "2001:db8::1%" + name,
                "fec0::1%" + name, "ff05::1%" + name, "fe80::1%" + name + " " };
        for (bool keep_bad_scope : readings) {
            for (const std::string& s : others) {
                INFO("keep_bad_scope " << keep_bad_scope << ", " << s);
                uint32_t scope = 99;
                CHECK(scope_of(s, keep_bad_scope, &scope) == keep_bad_scope);
                if (keep_bad_scope) {
                    CHECK(scope == 0);
                }
            }
        }
    }
    SECTION("but never what isn't an IPv6 address") {
        for (bool keep_bad_scope : readings) {
            for (const char* s : { "%1", "%", "1.2.3.4%1", "fe80::g%1", "fe80:::1%1" }) {
                INFO("keep_bad_scope " << keep_bad_scope << ", " << s);
                uint32_t scope = 99;
                CHECK_FALSE(scope_of(s, keep_bad_scope, &scope));
            }
        }
    }
    // Of the length given, not up to a NUL.
    _ebisu_dns_addr_t addr;
    CHECK(_ebisu_dns_probe_parse_address("fe80::1%12", 9, 53, false, &addr));
    CHECK(addr.scope_id == 1);
    CHECK_FALSE(_ebisu_dns_probe_parse_address("fe80::1%1\0", 10, 53, true, &addr));
}

TEST_CASE("A probe asks a server repeated in resolv.conf once") {
    FakeServer silent(FakeServer::SILENT);
    FakeServer server(FakeServer::ANSWER);
    REQUIRE(silent.listening);
    REQUIRE(server.listening);
    _ebisu_dns_addr_t servers[3];
    REQUIRE(_ebisu_dns_probe_parse_address("127.0.0.1", 9, silent.port, false, &servers[0]));
    servers[1] = servers[0];
    // The same address on another port is another server.
    REQUIRE(_ebisu_dns_probe_parse_address("127.0.0.1", 9, server.port, false, &servers[2]));
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];
    CHECK(_ebisu_dns_probe_ask_servers("example.com", servers, 3, 300, detail,
            sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
    CHECK(std::string(detail) == at(server.port) + " answered");
    CHECK(silent.received().size() == 1);
    CHECK(server.received().size() == 1);

    CHECK(_ebisu_dns_probe_ask_servers("127.0.0.1", servers, 3, 300, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_NOT_NEEDED);
    CHECK(std::string(detail) == "no lookup needed");
    CHECK(_ebisu_dns_probe_ask_servers("example.com", servers, 3, 300, nullptr, 0) ==
            EBISU_DNS_PROBE_ANSWERED);

    SECTION("but the same address on another interface is another server") {
        FakeServer silent6(FakeServer::SILENT, AF_INET6);
        if (!silent6.listening) {
            WARN("No IPv6 loopback here.");
            return;
        }
        // The loopback's address, which a scope doesn't change the way to.
        const std::string on_interface = "::1%" + std::to_string(an_interface().second);
        _ebisu_dns_addr_t two[2];
        REQUIRE(_ebisu_dns_probe_parse_address(on_interface.data(), on_interface.size(),
                silent6.port, false, &two[0]));
        REQUIRE(two[0].scope_id != 0);
        REQUIRE(_ebisu_dns_probe_parse_address("::1", 3, silent6.port, false, &two[1]));
        CHECK(_ebisu_dns_probe_ask_servers("example.com", two, 2, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(silent6.received().size() == 2);
    }
}

TEST_CASE("A probe asks no server for a host that needs none, and none without servers") {
    FakeServer server(FakeServer::ANSWER);
    REQUIRE(server.listening);
    const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    for (const char* host : { "127.0.0.1", "::1", "localhost", "" }) {
        INFO(host);
        CHECK(ebisu_dns_probe_servers(host, servers, 1, 1000, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_NOT_NEEDED);
        CHECK(std::string(detail) == "no lookup needed");
        CHECK(ebisu_dns_probe(host, 1000, detail, sizeof(detail)) == EBISU_DNS_PROBE_NOT_NEEDED);
    }
    CHECK(server.received().empty());

    CHECK(ebisu_dns_probe_servers("example.com", servers, 0, 1000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_SILENT);
    CHECK(std::string(detail) == "no server to ask");
    CHECK(ebisu_dns_probe_servers("example.com", nullptr, 1, 1000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_SILENT);
    CHECK(server.received().empty());
}

TEST_CASE("A probe asks a server") {
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    SECTION("which answers") {
        FakeServer server(FakeServer::ANSWER);
        REQUIRE(server.listening);
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(std::string(detail) == at(server.port) + " answered");
        std::vector<std::string> queries = server.received();
        REQUIRE(queries.size() == 1);
        uint16_t id = (uint16_t)(((unsigned char)queries[0][0] << 8) |
                (unsigned char)queries[0][1]);
        CHECK(queries[0] == query_for("example.com", id));

        SECTION("with another identifier for the next query") {
            CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, nullptr, 0) ==
                    EBISU_DNS_PROBE_ANSWERED);
            queries = server.received();
            REQUIRE(queries.size() == 2);
            CHECK(queries[1].substr(0, 2) != queries[0].substr(0, 2));
        }
        SECTION("cutting the detail short to fit") {
            char shorter[8];
            CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, shorter,
                    sizeof(shorter)) == EBISU_DNS_PROBE_ANSWERED);
            CHECK(std::string(shorter) == "127.0.0");
        }
    }
    SECTION("which sends something else first, then answers") {
        FakeServer server(FakeServer::OTHERS_FIRST);
        REQUIRE(server.listening);
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(std::string(detail) == at(server.port) + " answered");
    }
    SECTION("which sends only something else, within the time given") {
        FakeServer server(FakeServer::OTHERS_ONLY);
        REQUIRE(server.listening);
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 1000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        int64_t took = elapsed_ms(start);
        CHECK(took >= 1000);
        // Not the time given again after a datagram that isn't the answer.
        CHECK(took < 1000 + 400);
        CHECK(std::string(detail) == at(server.port) + " didn't answer in time");
    }
    SECTION("which doesn't answer, for the time given and no more") {
        FakeServer server(FakeServer::SILENT);
        REQUIRE(server.listening);
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        int64_t took = elapsed_ms(start);
        CHECK(took >= 300);
        CHECK(took < 300 + 1500);
        CHECK(std::string(detail) == at(server.port) + " didn't answer in time");
        CHECK(server.received().size() == 1);
    }
    SECTION("where nothing listens") {
        uint16_t port = unused_port();
        REQUIRE(port != 0);
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", port } };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(elapsed_ms(start) < 5000);
        CHECK(std::string(detail) == at(port) + " refused: nothing listens there");
    }
    SECTION("which isn't an address") {
        const ebisu_dns_server_t servers[] = { { "dns.example.com", 53 } };
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(std::string(detail) == "dns.example.com isn't an address");
        const ebisu_dns_server_t none[] = { { nullptr, 53 } };
        CHECK(ebisu_dns_probe_servers("example.com", none, 1, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(std::string(detail) == "NULL isn't an address");
        const ebisu_dns_server_t scoped[] = { { "fe80::1%no-such-if0", 53 } };
        CHECK(ebisu_dns_probe_servers("example.com", scoped, 1, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(std::string(detail) == "fe80::1%no-such-if0 isn't an address");
    }
}

TEST_CASE("A probe asks a server over IPv6") {
    FakeServer server(FakeServer::ANSWER, AF_INET6);
    if (!server.listening) {
        WARN("No IPv6 loopback here.");
        return;
    }
    const ebisu_dns_server_t servers[] = { { "::1", server.port } };
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];
    CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_ANSWERED);
    CHECK(std::string(detail) == "[::1]:" + std::to_string(server.port) + " answered");
}

TEST_CASE("A probe asks a link-local server on its interface") {
    sockaddr_in6 link_local;
    std::string name;
    if (!a_link_local(&link_local, &name)) {
        WARN("No link-local IPv6 address here.");
        return;
    }
    FakeServer server(FakeServer::ANSWER, link_local);
    REQUIRE(server.listening);
    char address[INET6_ADDRSTRLEN];
    REQUIRE(inet_ntop(AF_INET6, &link_local.sin6_addr, address, sizeof(address)) != nullptr);
    const std::string port = std::to_string(server.port);
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];
    for (const std::string& scope : { name, std::to_string(link_local.sin6_scope_id) }) {
        const std::string scoped = std::string(address) + "%" + scope;
        INFO(scoped);
        const ebisu_dns_server_t servers[] = { { scoped.c_str(), server.port } };
        CHECK(ebisu_dns_probe_servers("example.com", servers, 1, 5000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(std::string(detail) ==
                "[" + std::string(address) + "%" + name + "]:" + port + " answered");
    }
    // Without its interface, a link-local address can't be asked.
    const ebisu_dns_server_t unscoped[] = { { address, server.port } };
    CHECK(ebisu_dns_probe_servers("example.com", unscoped, 1, 5000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_SILENT);
    CHECK(std::string(detail) ==
            "[" + std::string(address) + "]:" + port + " failed: " + strerror(EINVAL));
    CHECK(server.received().size() == 2);
}

TEST_CASE("A probe asks the servers in turn, until one answers") {
    FakeServer silent(FakeServer::SILENT);
    FakeServer silent_too(FakeServer::SILENT);
    FakeServer answering(FakeServer::ANSWER);
    REQUIRE(silent.listening);
    REQUIRE(silent_too.listening);
    REQUIRE(answering.listening);
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    SECTION("the second when the first doesn't") {
        const ebisu_dns_server_t servers[] = {
            { "127.0.0.1", silent.port }, { "127.0.0.1", answering.port },
            { "127.0.0.1", silent_too.port }
        };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 3, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(elapsed_ms(start) >= 300);
        CHECK(std::string(detail) == at(answering.port) + " answered");
        CHECK(silent.received().size() == 1);
        CHECK(answering.received().size() == 1);
        CHECK(silent_too.received().empty());
        // The same query, to each.
        CHECK(silent.received()[0] == answering.received()[0]);
    }
    SECTION("past one that isn't an address, and one where nothing listens") {
        const ebisu_dns_server_t servers[] = {
            { "nowhere", 53 }, { "127.0.0.1", unused_port() }, { "127.0.0.1", answering.port }
        };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 3, 5000, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(elapsed_ms(start) < 5000);
        CHECK(std::string(detail) == at(answering.port) + " answered");
    }
    SECTION("none, after each had its time, saying what the last did") {
        const ebisu_dns_server_t servers[] = {
            { "127.0.0.1", silent.port }, { "127.0.0.1", silent_too.port }
        };
        auto start = std::chrono::steady_clock::now();
        CHECK(ebisu_dns_probe_servers("example.com", servers, 2, 300, detail,
                sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        int64_t took = elapsed_ms(start);
        CHECK(took >= 2 * 300);
        CHECK(took < 2 * 300 + 1500);
        CHECK(std::string(detail) == at(silent_too.port) + " didn't answer in time");
        CHECK(silent.received().size() == 1);
        CHECK(silent_too.received().size() == 1);
    }
    SECTION("none for an address") {
        const ebisu_dns_server_t servers[] = { { "127.0.0.1", silent.port } };
        CHECK(ebisu_dns_probe_servers("10.0.0.1", servers, 1, 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_NOT_NEEDED);
        CHECK(silent.received().empty());
    }
}

TEST_CASE("A probing socket connects the socket inside when a server answers") {
    FakeServer server(FakeServer::ANSWER);
    REQUIRE(server.listening);
    const ebisu_dns_server_t servers[] = { { "127.0.0.1", server.port } };
    Inner inner(true, true);
    ebisu_dns_probe_socket_t sock;
    ebisu_dns_probe_socket_init(&sock, &inner.sock.base, 5000);
    ebisu_dns_probe_socket_set_servers(&sock, servers, 1);
    const ebisu_socket_ops_t* ops = sock.base.ops;

    REQUIRE(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_OK);
    CHECK(server.received().size() == 1);
    CHECK(inner.connects == 1);
    CHECK(inner.host == "example.com");
    CHECK(inner.port == 443);

    // The rest is the inner socket's.
    size_t length = 0;
    char buffer[4];
    CHECK(ops->send(&sock.base, "abc", 3, &length) == EBISU_SOCK_OK);
    CHECK(length == 3);
    CHECK(inner.sends == 1);
    CHECK(ops->recv(&sock.base, buffer, sizeof(buffer), &length) == EBISU_SOCK_OK);
    CHECK(length == 1);
    CHECK(buffer[0] == 'x');
    CHECK(inner.recvs == 1);
    REQUIRE(ops->recv_timed_out != nullptr);
    CHECK(ops->recv_timed_out(&sock.base));
    CHECK(inner.timed_out_asked == 1);
    REQUIRE(ops->recv_wait != nullptr);
    CHECK(ops->recv_wait(&sock.base, 100));
    CHECK(inner.waits == 1);
    CHECK(inner.wait_ms == 100);
    long detail = 0;
    CHECK(ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_CONNECT);
    CHECK(detail == 111);
    CHECK(ops->close(&sock.base) == EBISU_SOCK_OK);
    CHECK(inner.closes == 1);

    SECTION("probing once for a connect that goes on") {
        inner.connect_codes = { EBISU_SOCK_AGAIN, EBISU_SOCK_AGAIN, EBISU_SOCK_OK };
        inner.connects = 0;
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_AGAIN);
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_AGAIN);
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_OK);
        CHECK(inner.connects == 3);
        CHECK(server.received().size() == 2);
        CHECK(ops->close(&sock.base) == EBISU_SOCK_OK);
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_OK);
        CHECK(server.received().size() == 3);
    }
    SECTION("and its failure, which the inner socket tells") {
        inner.connect_codes = { EBISU_SOCK_FAIL };
        inner.connects = 0;
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_FAIL);
        CHECK(ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_CONNECT);
        CHECK(ops->close(&sock.base) == EBISU_SOCK_OK);
        CHECK(inner.closes == 2);
    }
    SECTION("without asking for a host that needs no server") {
        CHECK(ops->connect(&sock.base, "127.0.0.1", 443) == EBISU_SOCK_OK);
        CHECK(server.received().size() == 1);
        CHECK(inner.connects == 2);
    }
}

TEST_CASE("A probing socket doesn't connect when no server answers") {
    FakeServer silent(FakeServer::SILENT);
    FakeServer answering(FakeServer::ANSWER);
    REQUIRE(silent.listening);
    REQUIRE(answering.listening);
    ebisu_dns_server_t servers[] = { { "127.0.0.1", silent.port } };
    std::vector<std::pair<ebisu_log_level_t, std::string> > lines;
    Lines logger = { { &keep_line_ops }, &lines };
    Inner inner(true, true);
    ebisu_dns_probe_socket_t sock;
    ebisu_dns_probe_socket_init(&sock, &inner.sock.base, 200);
    ebisu_dns_probe_socket_set_servers(&sock, servers, 1);
    ebisu_dns_probe_socket_set_logger(&sock, &logger.base);
    const ebisu_socket_ops_t* ops = sock.base.ops;

    CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_FAIL);
    CHECK(inner.connects == 0);
    long detail = 0;
    CHECK(ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS);
    CHECK(detail == EAI_AGAIN);
    CHECK(inner.errors_asked == 0);
    // Nothing to close.
    CHECK(ops->close(&sock.base) == EBISU_SOCK_OK);
    CHECK(inner.closes == 0);
#ifndef EBISU_NO_LOG
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].first == EBISU_LOG_ERROR);
    CHECK(lines[0].second == "failed to look up example.com: no name server answered: " +
            at(silent.port) + " didn't answer in time.");
#endif

    SECTION("then connects once one answers, saying so once") {
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_FAIL);
        servers[0].port = answering.port;
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_OK);
        CHECK(inner.connects == 1);
        CHECK(ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_CONNECT);
        CHECK(ops->close(&sock.base) == EBISU_SOCK_OK);
        CHECK(inner.closes == 1);
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_OK);
#ifndef EBISU_NO_LOG
        REQUIRE(lines.size() == 3);
        CHECK(lines[2].first == EBISU_LOG_INFO);
        CHECK(lines[2].second == "a name server answers again, after 2 failed connects: " +
                at(answering.port) + " answered.");
#endif
    }
    SECTION("nor with no server to ask") {
        ebisu_dns_probe_socket_set_servers(&sock, servers, 0);
        CHECK(ops->connect(&sock.base, "example.com", 443) == EBISU_SOCK_FAIL);
        CHECK(inner.connects == 0);
#ifndef EBISU_NO_LOG
        REQUIRE(lines.size() == 2);
        CHECK(lines[1].second ==
                "failed to look up example.com: no name server answered: no server to ask.");
#endif
    }
}

TEST_CASE("A probing socket has recv_timed_out and recv_wait if the socket inside has them") {
    for (int with = 0; with < 4; ++with) {
        Inner inner((with & 1) != 0, (with & 2) != 0);
        ebisu_dns_probe_socket_t sock;
        ebisu_dns_probe_socket_init(&sock, &inner.sock.base, 1000);
        INFO(with);
        CHECK((sock.base.ops->recv_timed_out != nullptr) == ((with & 1) != 0));
        CHECK((sock.base.ops->recv_wait != nullptr) == ((with & 2) != 0));
        CHECK(sock.base.ops->last_error != nullptr);
    }

    SECTION("and tells nothing of a failure the socket inside can't") {
        Inner inner(false, false, false);
        ebisu_dns_probe_socket_t sock;
        ebisu_dns_probe_socket_init(&sock, &inner.sock.base, 1000);
        inner.connect_codes = { EBISU_SOCK_FAIL };
        CHECK(sock.base.ops->connect(&sock.base, "10.0.0.1", 443) == EBISU_SOCK_FAIL);
        long detail = 1;
        CHECK(sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_NONE);
        CHECK(detail == 0);
    }
}
