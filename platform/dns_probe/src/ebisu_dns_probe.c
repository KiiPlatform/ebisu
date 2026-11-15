#include "ebisu_dns_probe.h"
#include "ebisu_dns_probe_impl.h"

#include <ebisu_object.h>

#include <stdio.h>
#include <string.h>

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

static char to_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
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
    ebisu_dns_addr_t addr;
    size_t length;
    if (host == NULL || !is_carried(host, &length)) {
        return false;
    }
    /* An address, an IPv6 one with any scope too, which the C libraries either
       take or refuse at once. */
    if (ebisu_dns_parse_address(NULL, host, strlen(host), 0, true, &addr)) {
        return false;
    }
    return !is_localhost(host, length);
}

static bool same_server(const ebisu_dns_addr_t* a, const ebisu_dns_addr_t* b)
{
    return a->family == b->family && a->port == b->port && a->scope_id == b->scope_id &&
        memcmp(a->bytes, b->bytes, sizeof(a->bytes)) == 0;
}

/* An identifier for each probe, for its answer to be told from other
   datagrams: different for each in a row, and, where the address of a static
   differs from process to process, for each process. */
static uint16_t next_id(void)
{
    static unsigned int probes;
    unsigned int n;
    unsigned int salt = (unsigned int)((uintptr_t)&probes >> 4);
#if defined(__GNUC__) || defined(__clang__)
    n = __atomic_fetch_add(&probes, 1u, __ATOMIC_RELAXED);
#else
    /* Two probes at once may take the same, which only makes their answers
       harder to tell from a stray datagram on another socket. */
    n = probes++;
#endif
    /* An odd factor, so that 65536 probes in a row have each an identifier of
       its own. */
    return (uint16_t)((n * 40503u + salt * 2654435761u) & 0xffff);
}

/* ebisu_dns_is_answer_t for a query with the identifier userdata points to. */
static bool answers_query(const unsigned char* datagram, size_t length, void* userdata)
{
    return _ebisu_dns_probe_is_answer(datagram, length, *(const uint16_t*)userdata);
}

/* Asks each of count servers in turn, as addresses in addrs, but one an
   earlier one repeats, or as given in given, until one answers. detail has
   room for at least its NUL. */
static ebisu_dns_probe_result_t ask_each(ebisu_dns_t* dns, const char* host,
        const ebisu_dns_addr_t* addrs, const ebisu_dns_server_t* given, size_t count,
        uint32_t timeout_ms, char* detail, size_t detail_size)
{
    unsigned char query[_EBISU_DNS_QUERY_MAX];
    uint16_t id = next_id();
    size_t query_length = _ebisu_dns_probe_build_query(host, id, query, sizeof(query));
    size_t i;
    snprintf(detail, detail_size, "no server to ask");
    for (i = 0; i < count; ++i) {
        ebisu_dns_addr_t server;
        char name[EBISU_DNS_ADDRESS_TEXT_SIZE + 64];
        char why[128];
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
        } else if (given[i].address == NULL || !ebisu_dns_parse_address(dns,
                given[i].address, strlen(given[i].address), given[i].port, false, &server)) {
            snprintf(detail, detail_size, "%s isn't an address",
                    given[i].address != NULL ? given[i].address : "NULL");
            continue;
        }
        ebisu_dns_format_address(dns, &server, name, sizeof(name));
        why[0] = '\0';
        switch (dns->ops->ask(dns, &server, query, query_length, timeout_ms, answers_query, &id,
                    why, sizeof(why))) {
        case EBISU_DNS_ASKED_ANSWERED:
            snprintf(detail, detail_size, "%s answered", name);
            return EBISU_DNS_PROBE_ANSWERED;
        case EBISU_DNS_ASKED_TIMEOUT:
            snprintf(detail, detail_size, "%s didn't answer in time", name);
            break;
        case EBISU_DNS_ASKED_REFUSED:
            snprintf(detail, detail_size, "%s refused: nothing listens there", name);
            break;
        default:
            why[sizeof(why) - 1] = '\0';
            snprintf(detail, detail_size, "%s failed: %s", name, why);
            break;
        }
    }
    return EBISU_DNS_PROBE_SILENT;
}

ebisu_dns_probe_result_t _ebisu_dns_probe_ask_servers(ebisu_dns_t* dns, const char* host,
        const ebisu_dns_addr_t* servers, size_t count, uint32_t timeout_ms, char* detail,
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
    return ask_each(dns, host, servers, NULL, count, timeout_ms, detail, detail_size);
}

ebisu_dns_probe_result_t ebisu_dns_probe(
        ebisu_dns_t* dns,
        const char* host,
        uint32_t timeout_ms,
        char* detail,
        size_t detail_size)
{
    ebisu_dns_addr_t servers[_EBISU_DNS_SERVERS_MAX];
    /* The system's servers only for a host that needs one. */
    size_t count = _ebisu_dns_probe_needs_lookup(host)
        ? dns->ops->servers(dns, servers, _EBISU_DNS_SERVERS_MAX) : 0;
    if (count > _EBISU_DNS_SERVERS_MAX) {
        count = _EBISU_DNS_SERVERS_MAX;
    }
    return _ebisu_dns_probe_ask_servers(dns, host, servers, count, timeout_ms, detail,
            detail_size);
}

ebisu_dns_probe_result_t ebisu_dns_probe_servers(
        ebisu_dns_t* dns,
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
    return ask_each(dns, host, NULL, servers, servers != NULL ? count : 0, timeout_ms, detail,
            detail_size);
}

static ebisu_sock_code_t probe_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_dns_probe_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_dns_probe_socket_t, base);
    ebisu_sock_code_t code;
    if (!sock->_connecting) {
        char detail[EBISU_DNS_PROBE_DETAIL_SIZE];
        ebisu_dns_probe_result_t result = sock->_servers != NULL
            ? ebisu_dns_probe_servers(sock->_dns, host, sock->_servers, sock->_server_count,
                    sock->_timeout_ms, detail, sizeof(detail))
            : ebisu_dns_probe(sock->_dns, host, sock->_timeout_ms, detail, sizeof(detail));
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
        *out_detail = sock->_dns->ops->no_answer_error != NULL
            ? sock->_dns->ops->no_answer_error(sock->_dns) : 0;
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
        ebisu_dns_t* dns,
        uint32_t timeout_ms)
{
    int ops = (inner->ops->recv_timed_out != NULL ? 1 : 0) +
        (inner->ops->recv_wait != NULL ? 2 : 0);
    sock->base.ops = &probe_socket_ops[ops];
    sock->_inner = inner;
    sock->_dns = dns;
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
