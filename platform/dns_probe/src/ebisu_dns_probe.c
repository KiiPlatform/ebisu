#include "ebisu_dns_probe.h"
#include "ebisu_dns_probe_impl.h"

#include <ebisu_object.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

/* Where the system's lookups find their name servers. */
#define RESOLV_CONF "/etc/resolv.conf"

/* The port name servers listen on. */
#define DNS_PORT 53

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* What isspace() takes in the C locale. */
static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* What splits a line of /etc/resolv.conf: a space or a tab for glibc, and what
   isspace() takes for musl. */
static bool is_blank(char c, bool as_glibc)
{
    return as_glibc ? c == ' ' || c == '\t' : is_space(c);
}

static char to_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

/* The value of the digit c, as a hexadecimal one, or -1. */
static int digit_value(char c)
{
    if (is_digit(c)) {
        return c - '0';
    }
    c = to_lower(c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* Whether a query can carry host, and if so its length without a trailing
   dot. */
static bool is_carried(const char* host, size_t* out_length)
{
    size_t length = strlen(host);
    size_t label = 0;
    size_t i;
    if (length > 0 && host[length - 1] == '.') {
        --length;
    }
    if (length == 0 || length > _EBISU_DNS_NAME_MAX) {
        return false;
    }
    for (i = 0; i < length; ++i) {
        if (host[i] != '.') {
            if (++label > _EBISU_DNS_LABEL_MAX) {
                return false;
            }
        } else if (label == 0) {
            return false;
        } else {
            label = 0;
        }
    }
    if (label == 0) {
        return false;
    }
    *out_length = length;
    return true;
}

size_t _ebisu_dns_probe_build_query(const char* host, uint16_t id, unsigned char* out,
        size_t size)
{
    size_t length;
    size_t label_at;
    size_t n;
    size_t i;
    if (host == NULL || !is_carried(host, &length) || size < 12 + length + 2 + 4) {
        return 0;
    }
    out[0] = (unsigned char)(id >> 8);
    out[1] = (unsigned char)(id & 0xff);
    /* A query, recursion desired. */
    out[2] = 0x01;
    out[3] = 0x00;
    /* One question, and nothing else. */
    out[4] = 0;
    out[5] = 1;
    memset(out + 6, 0, 6);
    /* Each label after its length, which is written once it is known. */
    label_at = 12;
    n = 13;
    for (i = 0; i < length; ++i) {
        if (host[i] == '.') {
            out[label_at] = (unsigned char)(n - label_at - 1);
            label_at = n++;
        } else {
            out[n++] = (unsigned char)host[i];
        }
    }
    out[label_at] = (unsigned char)(n - label_at - 1);
    out[n++] = 0;
    /* Type A, class IN. */
    out[n++] = 0;
    out[n++] = 1;
    out[n++] = 0;
    out[n++] = 1;
    return n;
}

bool _ebisu_dns_probe_is_answer(const unsigned char* datagram, size_t length, uint16_t id)
{
    return datagram != NULL && length >= 12 && datagram[0] == (unsigned char)(id >> 8) &&
        datagram[1] == (unsigned char)(id & 0xff) && (datagram[2] & 0x80) != 0;
}

/* Whether host, of length bytes without a trailing dot, is localhost or a name
   under it, which RFC 6761 has the system answer itself. */
static bool is_localhost(const char* host, size_t length)
{
    static const char localhost[] = "localhost";
    const size_t localhost_length = sizeof(localhost) - 1;
    size_t start;
    size_t i;
    if (length < localhost_length) {
        return false;
    }
    start = length - localhost_length;
    if (start > 0 && host[start - 1] != '.') {
        return false;
    }
    for (i = 0; i < localhost_length; ++i) {
        if (to_lower(host[start + i]) != localhost[i]) {
            return false;
        }
    }
    return true;
}

bool _ebisu_dns_probe_needs_lookup(const char* host)
{
    _ebisu_dns_addr_t addr;
    size_t length;
    if (host == NULL || !is_carried(host, &length)) {
        return false;
    }
    /* An address, an IPv6 one with any scope too, which the C libraries either
       take or refuse at once. */
    if (_ebisu_dns_probe_parse_address(host, strlen(host), 0, true, &addr)) {
        return false;
    }
    return !is_localhost(host, length);
}

bool _ebisu_dns_probe_parse_ipv4(const char* s, size_t length, unsigned char out[4])
{
    uint32_t parts[4];
    size_t count = 0;
    size_t i = 0;
    uint32_t value;
    /* Up to 4 numbers, each decimal, octal after a 0 or hexadecimal after 0x,
       between dots. */
    for (;;) {
        uint32_t base = 10;
        bool digits = false;
        if (i == length || !is_digit(s[i])) {
            return false;
        }
        value = 0;
        if (s[i] == '0') {
            ++i;
            if (i < length && (s[i] == 'x' || s[i] == 'X')) {
                base = 16;
                ++i;
            } else {
                base = 8;
                digits = true;
            }
        }
        for (; i < length; ++i) {
            int digit = digit_value(s[i]);
            if (digit < 0 || (uint32_t)digit >= base) {
                break;
            }
            if (value > (UINT32_MAX - (uint32_t)digit) / base) {
                return false;
            }
            value = value * base + (uint32_t)digit;
            digits = true;
        }
        if (!digits) {
            return false;
        }
        parts[count++] = value;
        if (i < length && s[i] == '.' && count < 4) {
            ++i;
        } else {
            break;
        }
    }
    if (i != length) {
        return false;
    }
    /* The last number fills the bytes the others leave. */
    for (i = 0; i + 1 < count; ++i) {
        if (parts[i] > 0xff) {
            return false;
        }
    }
    if (count < 4 && parts[count - 1] > (UINT32_MAX >> (8 * (count - 1)))) {
        return false;
    }
    if (count == 4 && parts[3] > 0xff) {
        return false;
    }
    value = parts[count - 1];
    for (i = 0; i + 1 < count; ++i) {
        value |= parts[i] << (8 * (3 - i));
    }
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)((value >> 16) & 0xff);
    out[2] = (unsigned char)((value >> 8) & 0xff);
    out[3] = (unsigned char)(value & 0xff);
    return true;
}

/* Whether the IPv6 address bytes is link-local, unicast or multicast: the
   addresses whose scope both C libraries take an interface's name for. */
static bool is_link_local(const unsigned char bytes[16])
{
    return (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) ||
        (bytes[0] == 0xff && (bytes[1] & 0x0f) == 0x02);
}

/* The scope s, of length bytes without a NUL, of the IPv6 address in out, in
   its scope_id. Whether it is one: an interface's number, in decimal digits
   only, or, for a link-local address, an interface's name. */
static bool parse_scope(const char* s, size_t length, _ebisu_dns_addr_t* out)
{
    char name[IF_NAMESIZE];
    uint32_t value = 0;
    size_t i;
    if (length == 0) {
        return false;
    }
    for (i = 0; i < length && is_digit(s[i]); ++i) {
        uint32_t digit = (uint32_t)(s[i] - '0');
        if (value > (UINT32_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    if (i == length) {
        out->scope_id = value;
        return true;
    }
    if (!is_link_local(out->bytes) || length >= sizeof(name)) {
        return false;
    }
    memcpy(name, s, length);
    name[length] = '\0';
    out->scope_id = if_nametoindex(name);
    return out->scope_id != 0;
}

bool _ebisu_dns_probe_parse_address(const char* s, size_t length, uint16_t port,
        bool keep_bad_scope, _ebisu_dns_addr_t* out)
{
    char text[INET6_ADDRSTRLEN];
    const char* scope;
    size_t address_length;
    memset(out, 0, sizeof(*out));
    out->port = port;
    if (_ebisu_dns_probe_parse_ipv4(s, length, out->bytes)) {
        out->family = AF_INET;
        return true;
    }
    if (length == 0 || memchr(s, '\0', length) != NULL) {
        return false;
    }
    scope = (const char*)memchr(s, '%', length);
    address_length = scope != NULL ? (size_t)(scope - s) : length;
    if (address_length == 0 || address_length >= sizeof(text)) {
        return false;
    }
    memcpy(text, s, address_length);
    text[address_length] = '\0';
    if (inet_pton(AF_INET6, text, out->bytes) != 1) {
        return false;
    }
    out->family = AF_INET6;
    /* Left out, its scope_id is still 0. */
    if (scope != NULL && !parse_scope(scope + 1, length - address_length - 1, out)) {
        return keep_bad_scope;
    }
    return true;
}

static bool same_server(const _ebisu_dns_addr_t* a, const _ebisu_dns_addr_t* b)
{
    return a->family == b->family && a->port == b->port && a->scope_id == b->scope_id &&
        memcmp(a->bytes, b->bytes, sizeof(a->bytes)) == 0;
}

size_t _ebisu_dns_probe_parse_resolv_line(const char* line, size_t length,
        _ebisu_dns_addr_t* servers, size_t count, size_t max, bool as_glibc)
{
    static const char keyword[] = "nameserver";
    const size_t keyword_length = sizeof(keyword) - 1;
    size_t start;
    size_t end;
    /* The keyword at the start of the line, and blanks after it, as the C
       libraries have it. */
    if (count >= max || length <= keyword_length ||
            memcmp(line, keyword, keyword_length) != 0 ||
            !is_blank(line[keyword_length], as_glibc)) {
        return count;
    }
    start = keyword_length;
    while (start < length && is_blank(line[start], as_glibc)) {
        ++start;
    }
    /* The address ends where a blank starts, and what is after it is left. A
       '#' or a ';' right after it is part of it, as for the C libraries, and
       it then isn't one. */
    end = start;
    while (end < length && !is_blank(line[end], as_glibc)) {
        ++end;
    }
    if (!_ebisu_dns_probe_parse_address(line + start, end - start, DNS_PORT, as_glibc,
                &servers[count])) {
        return count;
    }
    return count + 1;
}

size_t _ebisu_dns_probe_parse_resolv_conf(const char* text, size_t length,
        _ebisu_dns_addr_t* servers, size_t max, bool as_glibc)
{
    size_t count = 0;
    size_t start = 0;
    while (start < length) {
        size_t end = start;
        while (end < length && text[end] != '\n') {
            ++end;
        }
        count = _ebisu_dns_probe_parse_resolv_line(text + start, end - start, servers, count,
                max, as_glibc);
        start = end + 1;
    }
    return count;
}

/* The servers of file's lines, as glibc reads them: each whole, however long,
   up to a NUL. */
static size_t read_whole_lines(FILE* file, _ebisu_dns_addr_t* servers, size_t max)
{
    size_t count = 0;
    char* line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) != -1) {
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n') {
            --length;
        }
        count = _ebisu_dns_probe_parse_resolv_line(line, length, servers, count, max, true);
    }
    free(line);
    return count;
}

/* The servers of file's lines, as musl reads them: in a buffer of 256 bytes,
   skipping a line longer than 254, its newline aside, which it can't tell from
   one cut short, the last too. */
static size_t read_short_lines(FILE* file, _ebisu_dns_addr_t* servers, size_t max)
{
    size_t count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n') {
            --length;
        } else if (!feof(file)) {
            int c;
            do {
                c = fgetc(file);
            } while (c != EOF && c != '\n');
            continue;
        }
        count = _ebisu_dns_probe_parse_resolv_line(line, length, servers, count, max, false);
    }
    return count;
}

size_t _ebisu_dns_probe_read_servers(const char* path, _ebisu_dns_addr_t* servers,
        size_t max, bool as_glibc)
{
    size_t count = 0;
    FILE* file = fopen(path, "r");
    if (file != NULL) {
        count = as_glibc ? read_whole_lines(file, servers, max)
            : read_short_lines(file, servers, max);
        fclose(file);
    }
    if (count == 0) {
        static const unsigned char loopback[4] = { 127, 0, 0, 1 };
        memset(&servers[0], 0, sizeof(servers[0]));
        servers[0].family = AF_INET;
        memcpy(servers[0].bytes, loopback, sizeof(loopback));
        servers[0].port = DNS_PORT;
        count = 1;
    }
    return count;
}

/* In microseconds, for a wait never to end before the time given, as one
   counted from a time cut to the millisecond could. */
static int64_t monotonic_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/* An identifier for each probe, for its answer to be told from other
   datagrams: different for each in a row, and for each process. */
static uint16_t next_id(void)
{
    static unsigned int probes;
    unsigned int n;
#if defined(__GNUC__) || defined(__clang__)
    n = __atomic_fetch_add(&probes, 1u, __ATOMIC_RELAXED);
#else
    n = probes++;
#endif
    /* An odd factor, so that 65536 probes in a row have each an identifier of
       its own. */
    return (uint16_t)((n * 40503u + (unsigned int)getpid() * 2654435761u) & 0xffff);
}

/* What a server did with the query. */
typedef enum {
    ASKED_ANSWERED,
    ASKED_TIMEOUT,
    /* The system was told nothing listens there, as ICMP's port unreachable
       says. */
    ASKED_REFUSED,
    /* The query couldn't be sent: no socket, no route. */
    ASKED_FAILED
} asked_t;

/* Sends query, with id, to server, and waits timeout_ms for its answer. What it
   did, with errno in out_errno for ASKED_FAILED. */
static asked_t ask(const _ebisu_dns_addr_t* server, const unsigned char* query,
        size_t query_length, uint16_t id, uint32_t timeout_ms, int* out_errno)
{
    struct sockaddr_storage addr;
    socklen_t addr_length;
    int64_t deadline_us;
    asked_t asked;
    int flags;
    int sock;

    memset(&addr, 0, sizeof(addr));
    if (server->family == AF_INET) {
        struct sockaddr_in* in = (struct sockaddr_in*)&addr;
        in->sin_family = AF_INET;
        in->sin_port = htons(server->port);
        memcpy(&in->sin_addr, server->bytes, 4);
        addr_length = sizeof(*in);
    } else {
        struct sockaddr_in6* in6 = (struct sockaddr_in6*)&addr;
        in6->sin6_family = AF_INET6;
        in6->sin6_port = htons(server->port);
        memcpy(&in6->sin6_addr, server->bytes, 16);
        in6->sin6_scope_id = server->scope_id;
        addr_length = sizeof(*in6);
    }

    *out_errno = 0;
    sock = socket(server->family, SOCK_DGRAM, 0);
    if (sock < 0) {
        *out_errno = errno;
        return ASKED_FAILED;
    }
    /* Connected, so that only the server's datagrams are received, and its
       port unreachable is told: ECONNREFUSED. Not waiting but in poll(), and
       not inherited by a program another task executes meanwhile. */
    flags = fcntl(sock, F_GETFL);
    if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1 ||
            fcntl(sock, F_SETFD, FD_CLOEXEC) == -1 ||
            connect(sock, (struct sockaddr*)&addr, addr_length) != 0 ||
            send(sock, query, query_length, 0) < 0) {
        *out_errno = errno;
        close(sock);
        return *out_errno == ECONNREFUSED ? ASKED_REFUSED : ASKED_FAILED;
    }

    deadline_us = monotonic_us() + (int64_t)timeout_ms * 1000;
    for (;;) {
        unsigned char datagram[512];
        struct pollfd p;
        /* In milliseconds, rounded up. */
        int64_t left = (deadline_us - monotonic_us() + 999) / 1000;
        ssize_t received;
        int ready;
        if (left <= 0) {
            asked = ASKED_TIMEOUT;
            break;
        }
        p.fd = sock;
        p.events = POLLIN;
        p.revents = 0;
        ready = poll(&p, 1, left > 60000 ? 60000 : (int)left);
        if (ready == 0 || (ready < 0 && errno == EINTR)) {
            continue;
        }
        if (ready < 0) {
            *out_errno = errno;
            asked = ASKED_FAILED;
            break;
        }
        received = recv(sock, datagram, sizeof(datagram), 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            *out_errno = errno;
            asked = errno == ECONNREFUSED ? ASKED_REFUSED : ASKED_FAILED;
            break;
        }
        if (_ebisu_dns_probe_is_answer(datagram, (size_t)received, id)) {
            asked = ASKED_ANSWERED;
            break;
        }
    }
    close(sock);
    return asked;
}

/* Room for a server as format_server() writes it, with its NUL. */
#define SERVER_TEXT_SIZE (1 + INET6_ADDRSTRLEN + 1 + IF_NAMESIZE + 2 + 5)

/* server as "address:port", or "[address]:port" or "[address%scope]:port" for
   IPv6, its scope the interface's name, or its number without one. */
static void format_server(const _ebisu_dns_addr_t* server, char* out, size_t size)
{
    char text[INET6_ADDRSTRLEN];
    char scope[1 + IF_NAMESIZE];
    if (inet_ntop(server->family, server->bytes, text, sizeof(text)) == NULL) {
        snprintf(text, sizeof(text), "?");
    }
    scope[0] = '\0';
    if (server->scope_id != 0) {
        char name[IF_NAMESIZE];
        if (if_indextoname(server->scope_id, name) != NULL) {
            snprintf(scope, sizeof(scope), "%%%s", name);
        } else {
            snprintf(scope, sizeof(scope), "%%%lu", (unsigned long)server->scope_id);
        }
    }
    snprintf(out, size, server->family == AF_INET6 ? "[%s%s]:%u" : "%s%s:%u", text, scope,
            (unsigned int)server->port);
}

/* Asks each of count servers in turn, as addresses in addrs, but one an
   earlier one repeats, or as given in given, until one answers. detail has
   room for at least its NUL. */
static ebisu_dns_probe_result_t ask_each(const char* host, const _ebisu_dns_addr_t* addrs,
        const ebisu_dns_server_t* given, size_t count, uint32_t timeout_ms, char* detail,
        size_t detail_size)
{
    unsigned char query[_EBISU_DNS_QUERY_MAX];
    uint16_t id = next_id();
    size_t query_length = _ebisu_dns_probe_build_query(host, id, query, sizeof(query));
    size_t i;
    snprintf(detail, detail_size, "no server to ask");
    for (i = 0; i < count; ++i) {
        _ebisu_dns_addr_t server;
        char name[SERVER_TEXT_SIZE];
        int error;
        if (given == NULL) {
            /* The C libraries ask a server again where it is repeated, which
               tells nothing more. */
            size_t j = 0;
            while (j < i && !same_server(&addrs[j], &addrs[i])) {
                ++j;
            }
            if (j < i) {
                continue;
            }
            server = addrs[i];
        } else if (given[i].address == NULL || !_ebisu_dns_probe_parse_address(
                given[i].address, strlen(given[i].address), given[i].port, false, &server)) {
            snprintf(detail, detail_size, "%s isn't an address",
                    given[i].address != NULL ? given[i].address : "NULL");
            continue;
        }
        format_server(&server, name, sizeof(name));
        switch (ask(&server, query, query_length, id, timeout_ms, &error)) {
        case ASKED_ANSWERED:
            snprintf(detail, detail_size, "%s answered", name);
            return EBISU_DNS_PROBE_ANSWERED;
        case ASKED_TIMEOUT:
            snprintf(detail, detail_size, "%s didn't answer in time", name);
            break;
        case ASKED_REFUSED:
            snprintf(detail, detail_size, "%s refused: nothing listens there", name);
            break;
        default:
            snprintf(detail, detail_size, "%s failed: %s", name, strerror(error));
            break;
        }
    }
    return EBISU_DNS_PROBE_SILENT;
}

ebisu_dns_probe_result_t _ebisu_dns_probe_ask_servers(const char* host,
        const _ebisu_dns_addr_t* servers, size_t count, uint32_t timeout_ms, char* detail,
        size_t detail_size)
{
    char no_detail[1];
    if (detail == NULL || detail_size == 0) {
        detail = no_detail;
        detail_size = sizeof(no_detail);
    }
    if (!_ebisu_dns_probe_needs_lookup(host)) {
        snprintf(detail, detail_size, "no lookup needed");
        return EBISU_DNS_PROBE_NOT_NEEDED;
    }
    return ask_each(host, servers, NULL, count, timeout_ms, detail, detail_size);
}

ebisu_dns_probe_result_t ebisu_dns_probe(
        const char* host,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size)
{
    _ebisu_dns_addr_t servers[_EBISU_DNS_SERVERS_MAX];
    /* The file only for a host that needs a server. */
    size_t count = _ebisu_dns_probe_needs_lookup(host)
        ? _ebisu_dns_probe_read_servers(RESOLV_CONF, servers, _EBISU_DNS_SERVERS_MAX,
                _EBISU_DNS_READ_AS_GLIBC) : 0;
    return _ebisu_dns_probe_ask_servers(host, servers, count, timeout_ms, detail, detail_size);
}

ebisu_dns_probe_result_t ebisu_dns_probe_servers(
        const char* host,
        const ebisu_dns_server_t* servers,
        size_t count,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size)
{
    char no_detail[1];
    if (detail == NULL || detail_size == 0) {
        detail = no_detail;
        detail_size = sizeof(no_detail);
    }
    if (!_ebisu_dns_probe_needs_lookup(host)) {
        snprintf(detail, detail_size, "no lookup needed");
        return EBISU_DNS_PROBE_NOT_NEEDED;
    }
    return ask_each(host, NULL, servers, servers != NULL ? count : 0, timeout_ms, detail,
            detail_size);
}

static ebisu_sock_code_t probe_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    ebisu_sock_code_t code;
    if (!sock->_connecting) {
        char detail[EBISU_DNS_PROBE_DETAIL_SIZE];
        ebisu_dns_probe_result_t result = sock->_servers != NULL
            ? ebisu_dns_probe_servers(host, sock->_servers, sock->_server_count,
                    sock->_timeout_ms, detail, sizeof(detail))
            : ebisu_dns_probe(host, sock->_timeout_ms, detail, sizeof(detail));
        sock->_silent = result == EBISU_DNS_PROBE_SILENT;
        if (sock->_silent) {
            ++sock->_failures;
            log_to(sock->_logger, EBISU_LOG_ERROR,
                    "failed to look up %s: no name server answered: %s.", host, detail);
            return EBISU_SOCK_FAIL;
        }
        if (result == EBISU_DNS_PROBE_ANSWERED && sock->_failures > 0) {
            log_to(sock->_logger, EBISU_LOG_INFO,
                    "a name server answers again, after %lu failed connects: %s.",
                    sock->_failures, detail);
            sock->_failures = 0;
        }
    }
    sock->_inner_used = true;
    code = sock->_inner->ops->connect(sock->_inner, host, port);
    sock->_connecting = code == EBISU_SOCK_AGAIN;
    return code;
}

static ebisu_sock_code_t probe_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    return sock->_inner->ops->send(sock->_inner, buffer, length, out_sent_length);
}

static ebisu_sock_code_t probe_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    return sock->_inner->ops->recv(sock->_inner, buffer, length_to_read, out_actual_length);
}

static ebisu_sock_code_t probe_close(ebisu_socket_t* self)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    ebisu_sock_code_t code;
    sock->_connecting = false;
    if (!sock->_inner_used) {
        return EBISU_SOCK_OK;
    }
    code = sock->_inner->ops->close(sock->_inner);
    if (code != EBISU_SOCK_AGAIN) {
        sock->_inner_used = false;
    }
    return code;
}

static bool probe_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    return sock->_inner->ops->recv_timed_out(sock->_inner);
}

static ebisu_sock_error_t probe_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    if (sock->_silent) {
        *out_detail = EAI_AGAIN;
        return EBISU_SOCK_ERROR_DNS;
    }
    if (sock->_inner->ops->last_error != NULL) {
        return sock->_inner->ops->last_error(sock->_inner, out_detail);
    }
    *out_detail = 0;
    return EBISU_SOCK_ERROR_NONE;
}

static bool probe_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    return sock->_inner->ops->recv_wait(sock->_inner, timeout_ms);
}

/* With recv_timed_out and recv_wait as the socket inside has them: 1 for the
   first, 2 for the second. */
static const ebisu_socket_ops_t probe_socket_ops[4] = {
    { probe_connect, probe_send, probe_recv, probe_close, NULL, probe_last_error, NULL },
    {
        probe_connect, probe_send, probe_recv, probe_close, probe_recv_timed_out,
        probe_last_error, NULL
    },
    {
        probe_connect, probe_send, probe_recv, probe_close, NULL, probe_last_error,
        probe_recv_wait
    },
    {
        probe_connect, probe_send, probe_recv, probe_close, probe_recv_timed_out,
        probe_last_error, probe_recv_wait
    }
};

void ebisu_dns_probe_socket_init(
        ebisu_dns_probe_socket_t* sock,
        ebisu_socket_t* inner,
        uint32_t timeout_ms)
{
    int ops = (inner->ops->recv_timed_out != NULL ? 1 : 0) +
        (inner->ops->recv_wait != NULL ? 2 : 0);
    sock->base.ops = &probe_socket_ops[ops];
    sock->_inner = inner;
    sock->_timeout_ms = timeout_ms;
    sock->_servers = NULL;
    sock->_server_count = 0;
    sock->_logger = NULL;
    sock->_connecting = false;
    sock->_inner_used = false;
    sock->_silent = false;
    sock->_failures = 0;
}

void ebisu_dns_probe_socket_set_servers(
        ebisu_dns_probe_socket_t* sock,
        const ebisu_dns_server_t* servers,
        size_t count)
{
    sock->_servers = servers;
    sock->_server_count = servers != NULL ? count : 0;
}

void ebisu_dns_probe_socket_set_logger(ebisu_dns_probe_socket_t* sock, ebisu_logger_t* logger)
{
    sock->_logger = logger;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
