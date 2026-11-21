// Core's ebisu_dns.h, the addresses it reads and writes, and the probe over
// name servers of the test's own, as an application has for a system of its
// own.
#include <catch.hpp>
#include <ebisu_dns.h>
#include <ebisu_dns_probe.h>
#include "ebisu_dns_probe_impl.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "test_callbacks.h"

#include <stdio.h>
#include <string.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace {

ebisu_dns_addr_t ipv6_of(const std::string& text, uint32_t scope_id = 0) {
    ebisu_dns_addr_t addr;
    memset(&addr, 0, sizeof(addr));
    addr.family = EBISU_DNS_IPV6;
    addr.port = 53;
    addr.scope_id = scope_id;
    REQUIRE(inet_pton(AF_INET6, text.c_str(), addr.bytes) == 1);
    return addr;
}

// Name servers of the test's own: the servers it is given, which answer, time
// out or fail as each is told to, interfaces by name, and the error for none
// answering, each of them there only if asked for.
struct TestDns;

// What the probe is given, which leads back to the test's name servers.
struct TestDnsHandle {
    ebisu_dns_t base;
    TestDns* owner;
};

struct TestDns {
    struct Answer {
        ebisu_dns_asked_t asked;
        std::string why;
    };

    TestDnsHandle handle;
    ebisu_dns_ops_t ops;
    std::vector<ebisu_dns_addr_t> listed;
    std::map<std::string, Answer> answers;
    std::map<std::string, uint32_t> interfaces;
    long no_answer = 0;
    int servers_asked = 0;
    // Called at each query, before its answer is looked up.
    std::function<void()> on_ask;
    std::vector<std::string> asked;
    std::vector<std::string> queries;

    static TestDns* of(ebisu_dns_t* self) {
        return reinterpret_cast<TestDnsHandle*>(self)->owner;
    }

    ebisu_dns_t* get() {
        return &handle.base;
    }

    static std::string text_of(ebisu_dns_t* self, const ebisu_dns_addr_t* server) {
        char text[EBISU_DNS_ADDRESS_TEXT_SIZE + 64];
        ebisu_dns_format_address(self, server, text, sizeof(text));
        return text;
    }

    static size_t servers(ebisu_dns_t* self, ebisu_dns_addr_t* out, size_t max) {
        TestDns* dns = of(self);
        ++dns->servers_asked;
        size_t count = dns->listed.size() < max ? dns->listed.size() : max;
        for (size_t i = 0; i < count; ++i) {
            out[i] = dns->listed[i];
        }
        return count;
    }

    // An answer is the query with QR, as a server's, and before it one with
    // another identifier, which is_answer has to refuse.
    static ebisu_dns_asked_t ask(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
            const unsigned char* query, size_t length, uint32_t, ebisu_dns_is_answer_t is_answer,
            void* userdata, char* why, size_t why_size) {
        TestDns* dns = of(self);
        std::string name = text_of(self, server);
        if (dns->on_ask) {
            dns->on_ask();
        }
        dns->asked.push_back(name);
        dns->queries.push_back(std::string((const char*)query, length));
        auto found = dns->answers.find(name);
        Answer answer = found != dns->answers.end() ? found->second
            : Answer{ EBISU_DNS_ASKED_TIMEOUT, "" };
        if (answer.asked == EBISU_DNS_ASKED_ANSWERED) {
            std::vector<unsigned char> reply(query, query + length);
            reply[2] |= 0x80;
            std::vector<unsigned char> other = reply;
            other[1] ^= 1;
            if (is_answer(other.data(), other.size(), userdata) ||
                    !is_answer(reply.data(), reply.size(), userdata)) {
                return EBISU_DNS_ASKED_TIMEOUT;
            }
        }
        snprintf(why, why_size, "%s", answer.why.c_str());
        return answer.asked;
    }

    static uint32_t scope_of(ebisu_dns_t* self, const char* name) {
        auto found = of(self)->interfaces.find(name);
        return found != of(self)->interfaces.end() ? found->second : 0;
    }

    static bool scope_name(ebisu_dns_t* self, uint32_t scope_id, char* out, size_t size) {
        for (const auto& i : of(self)->interfaces) {
            if (i.second == scope_id && i.first.size() < size) {
                memcpy(out, i.first.c_str(), i.first.size() + 1);
                return true;
            }
        }
        return false;
    }

    static long no_answer_error(ebisu_dns_t* self) {
        return of(self)->no_answer;
    }

    explicit TestDns(bool with_scopes = true, bool with_no_answer_error = true) {
        ops.servers = servers;
        ops.ask = ask;
        ops.scope_of = with_scopes ? scope_of : nullptr;
        ops.scope_name = with_scopes ? scope_name : nullptr;
        ops.no_answer_error = with_no_answer_error ? no_answer_error : nullptr;
        handle.base.ops = &ops;
        handle.owner = this;
    }
    TestDns(const TestDns&) = delete;
    TestDns& operator=(const TestDns&) = delete;

    void list(const std::string& address) {
        ebisu_dns_addr_t addr;
        REQUIRE(ebisu_dns_parse_address(get(), address.c_str(), address.size(), 53, false,
                &addr));
        listed.push_back(addr);
    }
};

std::string written(ebisu_dns_t* dns, const ebisu_dns_addr_t& addr, size_t size = 128) {
    std::vector<char> text(size);
    ebisu_dns_format_address(dns, &addr, text.data(), text.size());
    return text.data();
}

// A socket that connects, as the probing socket's inner one.
ebisu_sock_code_t connects(ebisu_socket_t*, const char*, uint16_t) {
    return EBISU_SOCK_OK;
}

} // namespace

// Only where inet_pton() is the C library's, which Windows' isn't.
#ifndef _WIN32
TEST_CASE("Core reads IPv6 addresses as inet_pton() does") {
    for (const char* s : { "::", "::1", "1::", "1::2", "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7::",
            "::2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8:9", "1:2:3:4:5:6:7", "1::2::3", ":::", ":1::",
            "1:", ":1", "12345::", "fffff::", "::ffff:1.2.3.4", "::1.2.3.4",
            "1:2:3:4:5:6:1.2.3.4", "1:2:3:4:5:6:7:1.2.3.4", "::1.2.3", "::1.2.3.4.5",
            "::01.2.3.4", "::256.1.1.1", "::1.2.3.04", "1::1.2.3.4", "g::", "::G", "FE80::1",
            "2001:DB8::", "1:2:3:4:5:6:7:8::", "::1:2:3:4:5:6:7", "::1:2:3:4:5:6:7:8",
            "0:0:0:0:0:0:0:0", "0000::", "::ffff:0.0.0.0", "1.2.3.4::", " ::1",
            "::1 ", "::1:", "1:::2", "::1.2.3.4:5", "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff",
            "::1.2.3.", "::.1.2.3", "1:2:3:4:5::1.2.3.4", "1:2:3:4:5:6::1.2.3.4" }) {
        INFO(s);
        unsigned char theirs[16];
        bool expected = inet_pton(AF_INET6, s, theirs) == 1;
        ebisu_dns_addr_t ours;
        bool parsed = ebisu_dns_parse_address(nullptr, s, strlen(s), 53, false, &ours) &&
            ours.family == EBISU_DNS_IPV6;
        REQUIRE(parsed == expected);
        if (parsed) {
            CHECK(memcmp(ours.bytes, theirs, 16) == 0);
            CHECK(ours.port == 53);
            CHECK(ours.scope_id == 0);
        }
    }
}
#endif

// RFC 4291's groups have 4 digits at most, which glibc 2.24, the armhf job's,
// doesn't hold to where the value fits in 16 bits, and the C libraries of the
// other jobs do.
TEST_CASE("Core reads an IPv6 group of more than 4 digits as no address") {
    for (const char* s : { "00000::", "::00001", "1:2:3:4:5:6:7:00008", "fffff::" }) {
        INFO(s);
        ebisu_dns_addr_t addr;
        CHECK_FALSE(ebisu_dns_parse_address(nullptr, s, strlen(s), 53, false, &addr));
    }
}

TEST_CASE("Core writes addresses as RFC 5952 says") {
    ebisu_dns_addr_t v4;
    memset(&v4, 0, sizeof(v4));
    v4.family = EBISU_DNS_IPV4;
    v4.bytes[0] = 192;
    v4.bytes[3] = 2;
    v4.port = 5353;
    CHECK(written(nullptr, v4) == "192.0.0.2:5353");

    for (const auto& c : std::vector<std::pair<std::string, std::string>>({
            { "::", "::" },
            { "::1", "::1" },
            { "1::", "1::" },
            { "2001:db8::1", "2001:db8::1" },
            { "2001:DB8::ABCD", "2001:db8::abcd" },
            { "2001:0db8:0000:0000:0001:0000:0000:0001", "2001:db8::1:0:0:1" },
            { "2001:0:0:1:0:0:0:1", "2001:0:0:1::1" },
            { "1:0:2:3:4:5:6:7", "1:0:2:3:4:5:6:7" },
            { "0:0:1:0:0:1:0:0", "::1:0:0:1:0:0" },
            { "1:0:0:2:0:0:3:4", "1::2:0:0:3:4" },
            { "::ffff:1.2.3.4", "::ffff:1.2.3.4" },
            { "1:2:3:4:5:6:7:8", "1:2:3:4:5:6:7:8" } })) {
        INFO(c.first);
        CHECK(written(nullptr, ipv6_of(c.first)) == "[" + c.second + "]:53");
    }

    SECTION("with a scope, as the name servers name it, or as a number") {
        TestDns dns;
        dns.interfaces["eth9"] = 9;
        CHECK(written(dns.get(), ipv6_of("fe80::1", 9)) == "[fe80::1%eth9]:53");
        CHECK(written(dns.get(), ipv6_of("fe80::1", 8)) == "[fe80::1%8]:53");
        CHECK(written(nullptr, ipv6_of("fe80::1", 9)) == "[fe80::1%9]:53");
        TestDns unnamed(false);
        CHECK(written(unnamed.get(), ipv6_of("fe80::1", 9)) == "[fe80::1%9]:53");
    }

    SECTION("cut short to fit, and terminated") {
        CHECK(written(nullptr, ipv6_of("2001:db8::1"), 8) == "[2001:d");
        CHECK(written(nullptr, v4, 4) == "192");
    }
}

// Whatever the groups of 0 are, what is written reads back as the address.
TEST_CASE("Core writes each IPv6 address so that it reads back as itself") {
    for (unsigned int zeros = 0; zeros < 256; ++zeros) {
        ebisu_dns_addr_t addr;
        memset(&addr, 0, sizeof(addr));
        addr.family = EBISU_DNS_IPV6;
        addr.port = 53;
        for (int group = 0; group < 8; ++group) {
            if ((zeros & (1u << group)) == 0) {
                addr.bytes[2 * group] = (unsigned char)(group * 17);
                addr.bytes[2 * group + 1] = (unsigned char)(0xa0 + group);
            }
        }
        std::string text = written(nullptr, addr);
        INFO(text);
        REQUIRE(text.size() > 5);
        std::string address = text.substr(1, text.find(']') - 1);
        unsigned char theirs[16];
        REQUIRE(inet_pton(AF_INET6, address.c_str(), theirs) == 1);
        CHECK(memcmp(theirs, addr.bytes, 16) == 0);
        ebisu_dns_addr_t ours;
        REQUIRE(ebisu_dns_parse_address(nullptr, address.c_str(), address.size(), 53, false,
                &ours));
        CHECK(memcmp(ours.bytes, addr.bytes, 16) == 0);
    }
}

TEST_CASE("Core reads an IPv6 address's scope, a name as the name servers number it") {
    TestDns dns;
    dns.interfaces["eth9"] = 9;
    ebisu_dns_addr_t addr;
    auto parse = [&](ebisu_dns_t* with, const std::string& s, bool keep_bad_scope) {
        return ebisu_dns_parse_address(with, s.c_str(), s.size(), 53, keep_bad_scope, &addr);
    };

    CHECK(parse(dns.get(), "fe80::1%eth9", false));
    CHECK(addr.scope_id == 9);
    CHECK(parse(dns.get(), "ff02::1%eth9", false));
    CHECK(addr.scope_id == 9);
    CHECK(parse(dns.get(), "2001:db8::1%12", false));
    CHECK(addr.scope_id == 12);

    // A name for an address that isn't link-local, an interface there is no
    // number for, or name servers without scope_of.
    for (const char* s : { "2001:db8::1%eth9", "fe80::1%eth8", "fe80::1%", "fe80::1%4294967296" }) {
        INFO(s);
        CHECK_FALSE(parse(dns.get(), s, false));
        REQUIRE(parse(dns.get(), s, true));
        CHECK(addr.scope_id == 0);
    }
    CHECK_FALSE(parse(nullptr, "fe80::1%eth9", false));
    TestDns unnamed(false);
    CHECK_FALSE(parse(unnamed.get(), "fe80::1%eth9", false));
    // Not after IPv4.
    CHECK_FALSE(parse(dns.get(), "127.0.0.1%1", true));
}

TEST_CASE("A probe asks name servers of the application's own") {
    TestDns dns;
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    SECTION("the first 3 they list, a repeated one once, until one answers") {
        for (const char* s : { "10.0.0.1", "10.0.0.1", "10.0.0.2", "10.0.0.3" }) {
            dns.list(s);
        }
        dns.answers["10.0.0.2:53"] = { EBISU_DNS_ASKED_TIMEOUT, "" };
        CHECK(ebisu_dns_probe(dns.get(), "example.com", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_SILENT);
        CHECK(dns.asked == std::vector<std::string>({ "10.0.0.1:53", "10.0.0.2:53" }));
        CHECK(std::string(detail) == "10.0.0.2:53 didn't answer in time");
        // The same query, to each.
        CHECK(dns.queries[0] == dns.queries[1]);

        dns.answers["10.0.0.2:53"] = { EBISU_DNS_ASKED_ANSWERED, "" };
        dns.asked.clear();
        CHECK(ebisu_dns_probe(dns.get(), "example.com", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_ANSWERED);
        CHECK(std::string(detail) == "10.0.0.2:53 answered");
    }

    SECTION("saying what each did") {
        dns.interfaces["eth9"] = 9;
        dns.list("10.0.0.1");
        dns.list("fe80::53%eth9");
        dns.list("2001:db8::53");
        dns.answers["10.0.0.1:53"] = { EBISU_DNS_ASKED_FAILED, "no route" };
        dns.answers["[fe80::53%eth9]:53"] = { EBISU_DNS_ASKED_REFUSED, "" };
        dns.answers["[2001:db8::53]:53"] = { EBISU_DNS_ASKED_FAILED, "no route either" };
        CHECK(ebisu_dns_probe(dns.get(), "example.com", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_SILENT);
        CHECK(dns.asked.size() == 3);
        CHECK(std::string(detail) == "[2001:db8::53]:53 failed: no route either");

        const ebisu_dns_server_t given[] = { { "fe80::53%eth9", 53 }, { "fe80::1%eth8", 53 } };
        CHECK(ebisu_dns_probe_servers(dns.get(), "example.com", given, 1, 300, detail,
                    sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(std::string(detail) == "[fe80::53%eth9]:53 refused: nothing listens there");
        CHECK(ebisu_dns_probe_servers(dns.get(), "example.com", given + 1, 1, 300, detail,
                    sizeof(detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(std::string(detail) == "fe80::1%eth8 isn't an address");
    }

    SECTION("none, for a host that needs none") {
        dns.list("10.0.0.1");
        CHECK(ebisu_dns_probe(dns.get(), "localhost", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_NOT_NEEDED);
        CHECK(dns.servers_asked == 0);
        CHECK(dns.asked.empty());
    }

    SECTION("and says no server answered without any") {
        CHECK(ebisu_dns_probe(dns.get(), "example.com", 300, detail, sizeof(detail)) ==
                EBISU_DNS_PROBE_SILENT);
        CHECK(dns.servers_asked == 1);
        CHECK(std::string(detail) == "no server to ask");
    }
}

TEST_CASE("A probing socket gives the name servers' error for none answering") {
    ebisu_socket_ops_t inner_ops = { connects, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr };
    ebisu_socket_t inner = { &inner_ops };
    long detail = -1;

    TestDns dns;
    dns.list("10.0.0.1");
    dns.no_answer = 4242;
    ebisu_dns_probe_socket_t sock;
    ebisu_dns_probe_socket_init(&sock, &inner, dns.get(), 300);
    CHECK(sock.base.ops->connect(&sock.base, "example.com", 1883) == EBISU_SOCK_FAIL);
    CHECK(sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS);
    CHECK(detail == 4242);

    SECTION("or 0 without no_answer_error") {
        TestDns plain(true, false);
        plain.list("10.0.0.1");
        ebisu_dns_probe_socket_init(&sock, &inner, plain.get(), 300);
        CHECK(sock.base.ops->connect(&sock.base, "example.com", 1883) == EBISU_SOCK_FAIL);
        CHECK(sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS);
        CHECK(detail == 0);
    }

    SECTION("and connects the socket inside once one answers") {
        dns.answers["10.0.0.1:53"] = { EBISU_DNS_ASKED_ANSWERED, "" };
        CHECK(sock.base.ops->connect(&sock.base, "example.com", 1883) == EBISU_SOCK_OK);
    }
}

namespace {

// A system whose clock the waits move on, and each probe by what it takes.
struct WaitWorld {
    TestDns dns;
    khct::cb::SystemCtx sys;
    uint64_t now_ms;
    std::vector<uint32_t> delays;
    // What a probe takes, and from which probe on, counted from 1, the server answers: 0 for never.
    uint32_t probe_takes_ms = 100;
    size_t answers_from = 0;
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    explicit WaitWorld(uint64_t start_ms = 5000) : now_ms(start_ms) {
        dns.list("10.0.0.1");
        sys.on_monotonic_ms = [this]() { return now_ms; };
        sys.on_delay_ms = [this](uint32_t ms) {
            delays.push_back(ms);
            now_ms += ms;
        };
        dns.on_ask = [this]() {
            now_ms += probe_takes_ms;
            bool answers = answers_from != 0 && dns.asked.size() + 1 >= answers_from;
            dns.answers["10.0.0.1:53"] = { answers ? EBISU_DNS_ASKED_ANSWERED : EBISU_DNS_ASKED_TIMEOUT, "" };
        };
    }

    ebisu_dns_probe_result_t wait(uint32_t every_ms, uint32_t max_ms, const char* host = "example.com") {
        return ebisu_dns_probe_wait(dns.get(), sys.system(), host, 300, every_ms, max_ms, detail,
                sizeof(detail));
    }

    size_t probes() const { return dns.asked.size(); }
};

}

TEST_CASE("A wait for the name servers probes until one answers, or for so long") {
    SECTION("a server that answers at once: one probe, no wait") {
        WaitWorld w;
        w.answers_from = 1;
        CHECK(w.wait(5000, 600000) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(w.probes() == 1);
        CHECK(w.delays.empty());
        CHECK(std::string(w.detail) == "10.0.0.1:53 answered");
    }

    SECTION("one that answers the third probe: two waits of every_ms, and the last probe's detail") {
        WaitWorld w;
        w.answers_from = 3;
        CHECK(w.wait(5000, 600000) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(w.probes() == 3);
        CHECK(w.delays == std::vector<uint32_t>({ 5000, 5000 }));
        CHECK(std::string(w.detail) == "10.0.0.1:53 answered");
    }

    SECTION("none that answers: the last wait cut to end at max_ms, a last probe, and no wait after it") {
        WaitWorld w;
        const uint64_t start = w.now_ms;
        CHECK(w.wait(5000, 12000) == EBISU_DNS_PROBE_SILENT);
        // Probes of 100 ms at 0, 5100 and 10200, and the last at 12000.
        CHECK(w.delays == std::vector<uint32_t>({ 5000, 5000, 1700 }));
        CHECK(w.probes() == 4);
        CHECK(w.now_ms - start == 12000 + 100);
        CHECK(std::string(w.detail) == "10.0.0.1:53 didn't answer in time");
    }

    SECTION("a probe that ends just at max_ms is the last") {
        WaitWorld w;
        CHECK(w.wait(4900, 10000) == EBISU_DNS_PROBE_SILENT);
        // Probes at 0 and 5000, the second ending at 5100: 4900 left, a wait, and the probe at 10000.
        CHECK(w.delays == std::vector<uint32_t>({ 4900, 4900 }));
        CHECK(w.probes() == 3);
    }

    SECTION("max_ms of 0: one probe, no wait") {
        WaitWorld w;
        CHECK(w.wait(5000, 0) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 1);
        CHECK(w.delays.empty());
    }

    SECTION("a first probe that takes max_ms or longer: one probe, no wait") {
        WaitWorld w;
        w.probe_takes_ms = 3000;
        CHECK(w.wait(1000, 3000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 1);
        CHECK(w.delays.empty());
        w.probe_takes_ms = 2999;
        w.dns.asked.clear();
        CHECK(w.wait(1000, 3000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 2);
        CHECK(w.delays == std::vector<uint32_t>({ 1 }));
    }

    SECTION("every_ms of 0: a probe at once after the last, with no delay asked of the system") {
        WaitWorld w;
        CHECK(w.wait(0, 1000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.delays.empty());
        // Of 100 ms each: the one that ends at 1000 is the last.
        CHECK(w.probes() == 10);
    }

    SECTION("an answer that the name doesn't exist is an answer, and a host that needs no server needs no wait") {
        WaitWorld w;
        CHECK(w.wait(5000, 60000, "127.0.0.1") == EBISU_DNS_PROBE_NOT_NEEDED);
        CHECK(w.probes() == 0);
        CHECK(w.delays.empty());
        CHECK(w.wait(5000, 60000, "localhost") == EBISU_DNS_PROBE_NOT_NEEDED);
        CHECK(w.probes() == 0);
    }

    SECTION("a clock near its end: the time gone by is a difference") {
        WaitWorld w(UINT64_MAX - 6000);
        CHECK(w.wait(5000, 12000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.delays == std::vector<uint32_t>({ 5000, 5000, 1700 }));
        CHECK(w.probes() == 4);
    }

    SECTION("the longest of each: no overflow") {
        WaitWorld w;
        w.answers_from = 2;
        CHECK(w.wait(UINT32_MAX, UINT32_MAX) == EBISU_DNS_PROBE_ANSWERED);
        CHECK(w.delays == std::vector<uint32_t>({ UINT32_MAX - 100 }));
    }

    SECTION("without a system, or one that can't tell the time or wait: one probe") {
        WaitWorld w;
        CHECK(ebisu_dns_probe_wait(w.dns.get(), nullptr, "example.com", 300, 5000, 60000, w.detail,
                    sizeof(w.detail)) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 1);
        w.sys.ops.monotonic_ms = nullptr;
        CHECK(w.wait(5000, 60000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 2);
        w.sys.ops.monotonic_ms = khct::cb::mock_monotonic_ms;
        w.sys.ops.delay_ms = nullptr;
        CHECK(w.wait(5000, 60000) == EBISU_DNS_PROBE_SILENT);
        CHECK(w.probes() == 3);
        CHECK(w.delays.empty());
        // And an answer is still one.
        w.answers_from = 1;
        CHECK(ebisu_dns_probe_wait(w.dns.get(), nullptr, "example.com", 300, 5000, 60000, nullptr, 0) ==
                EBISU_DNS_PROBE_ANSWERED);
    }
}
