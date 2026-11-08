// What the DNS probe reads and writes, from bytes the input chooses: the
// input as /etc/resolv.conf, read as musl does and as glibc does, whose
// servers are each an address the parser takes back, with its scope, and no
// more than the C libraries take; as a host,
// whose query, built in a buffer exactly its size and refused by one a byte
// shorter, carries the name back, label by label, and is answered only once
// QR is set; and as an IPv4 address, whose bytes, written as a dotted quad,
// read back the same. Each from memory of exactly its length, so that a read
// past it is an ASan finding.
#include "ebisu_dns_probe_impl.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <string>
#include <vector>

namespace {

void check_resolv_conf(const std::vector<char>& text, bool keep_bad_scope) {
    _ebisu_dns_addr_t servers[_EBISU_DNS_SERVERS_MAX];
    size_t count = _ebisu_dns_probe_parse_resolv_conf(text.empty() ? nullptr : text.data(),
            text.size(), servers, _EBISU_DNS_SERVERS_MAX, keep_bad_scope);
    if (count > _EBISU_DNS_SERVERS_MAX) abort();
    for (size_t i = 0; i < count; ++i) {
        if (servers[i].port != 53) abort();
        if (servers[i].family != AF_INET && servers[i].family != AF_INET6) abort();
        if (servers[i].family == AF_INET && servers[i].scope_id != 0) abort();
        char address[INET6_ADDRSTRLEN];
        if (inet_ntop(servers[i].family, servers[i].bytes, address, sizeof(address)) == nullptr) {
            abort();
        }
        // Its scope as a number, which any address takes, as musl reads it.
        std::string line = std::string("nameserver ") + address;
        if (servers[i].scope_id != 0) {
            line += "%" + std::to_string(servers[i].scope_id);
        }
        _ebisu_dns_addr_t back[1];
        if (_ebisu_dns_probe_parse_resolv_line(line.data(), line.size(), back, 0, 1, false) != 1 ||
                back[0].family != servers[i].family || back[0].scope_id != servers[i].scope_id ||
                memcmp(back[0].bytes, servers[i].bytes, 16) != 0) abort();
    }
}

void check_query(const std::vector<char>& bytes) {
    // The host, up to a NUL, as a C string is.
    std::string host = bytes.empty() ? std::string()
        : std::string(bytes.data(), strnlen(bytes.data(), bytes.size()));
    std::vector<unsigned char> room(_EBISU_DNS_QUERY_MAX);
    size_t length = _ebisu_dns_probe_build_query(host.c_str(), 0x1234, room.data(), room.size());
    if (length == 0) {
        if (_ebisu_dns_probe_needs_lookup(host.c_str())) abort();
        return;
    }
    std::string name = host;
    if (!name.empty() && name.back() == '.') {
        name.pop_back();
    }
    if (length != 12 + name.size() + 2 + 4) abort();
    std::vector<unsigned char> exact(length);
    if (_ebisu_dns_probe_build_query(host.c_str(), 0x1234, exact.data(), length) != length) abort();
    if (memcmp(exact.data(), room.data(), length) != 0) abort();
    std::vector<unsigned char> shorter(length - 1);
    if (_ebisu_dns_probe_build_query(host.c_str(), 0x1234, shorter.data(), shorter.size()) != 0) {
        abort();
    }
    // The labels, back to the name.
    std::string back;
    size_t at = 12;
    while (exact[at] != 0) {
        size_t label = exact[at];
        if (label > _EBISU_DNS_LABEL_MAX || at + 1 + label >= length) abort();
        if (!back.empty()) {
            back += '.';
        }
        back.append((const char*)&exact[at + 1], label);
        at += 1 + label;
    }
    if (back != name || at + 5 != length) abort();
    if (_ebisu_dns_probe_is_answer(exact.data(), length, 0x1234)) abort();
    exact[2] |= 0x80;
    if (!_ebisu_dns_probe_is_answer(exact.data(), length, 0x1234)) abort();
    if (_ebisu_dns_probe_is_answer(exact.data(), length, 0x1235)) abort();
}

void check_ipv4(const std::vector<char>& bytes) {
    unsigned char out[4];
    if (!_ebisu_dns_probe_parse_ipv4(bytes.empty() ? nullptr : bytes.data(), bytes.size(),
                out)) {
        return;
    }
    char quad[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, out, quad, sizeof(quad)) == nullptr) abort();
    unsigned char back[4];
    if (!_ebisu_dns_probe_parse_ipv4(quad, strlen(quad), back) ||
            memcmp(back, out, 4) != 0) abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Copies of exactly the input's length.
    const std::vector<char> bytes(data, data + size);
    check_resolv_conf(bytes, false);
    check_resolv_conf(bytes, true);
    check_query(bytes);
    check_ipv4(bytes);
    return 0;
}
