/* platform's POSIX port's name servers: those of /etc/resolv.conf, as the C
   libraries read it, asked over BSD sockets. */

#include "ebisu_posix.h"
#include "ebisu_posix_dns_impl.h"

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

/* Where the system's lookups find their name servers. */
#define RESOLV_CONF "/etc/resolv.conf"

/* The port name servers listen on. */
#define DNS_PORT 53

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


size_t _ebisu_posix_dns_parse_resolv_line(const char* line, size_t length,
        ebisu_dns_addr_t* servers, size_t count, size_t max, bool as_glibc)
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
    if (!ebisu_dns_parse_address(ebisu_posix_dns(), line + start, end - start, DNS_PORT,
                as_glibc, &servers[count])) {
        return count;
    }
    return count + 1;
}

size_t _ebisu_posix_dns_parse_resolv_conf(const char* text, size_t length,
        ebisu_dns_addr_t* servers, size_t max, bool as_glibc)
{
    size_t count = 0;
    size_t start = 0;
    while (start < length) {
        size_t end = start;
        while (end < length && text[end] != '\n') {
            ++end;
        }
        count = _ebisu_posix_dns_parse_resolv_line(text + start, end - start, servers, count,
                max, as_glibc);
        start = end + 1;
    }
    return count;
}

/* The servers of file's lines, as glibc reads them: each whole, however long,
   up to a NUL. */
static size_t read_whole_lines(FILE* file, ebisu_dns_addr_t* servers, size_t max)
{
    size_t count = 0;
    char* line = NULL;
    size_t size = 0;
    while (getline(&line, &size, file) != -1) {
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n') {
            --length;
        }
        count = _ebisu_posix_dns_parse_resolv_line(line, length, servers, count, max, true);
    }
    free(line);
    return count;
}

/* The servers of file's lines, as musl reads them: in a buffer of 256 bytes,
   skipping a line longer than 254, its newline aside, which it can't tell from
   one cut short, the last too. */
static size_t read_short_lines(FILE* file, ebisu_dns_addr_t* servers, size_t max)
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
        count = _ebisu_posix_dns_parse_resolv_line(line, length, servers, count, max, false);
    }
    return count;
}

size_t _ebisu_posix_dns_read_servers(const char* path, ebisu_dns_addr_t* servers,
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
        servers[0].family = EBISU_DNS_IPV4;
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

/* Sends query to server, and waits timeout_ms for a datagram is_answer takes.
   What it did, with errno in out_errno for EBISU_DNS_ASKED_FAILED. */
static ebisu_dns_asked_t ask_once(const ebisu_dns_addr_t* server, const unsigned char* query,
        size_t query_length, uint32_t timeout_ms, ebisu_dns_is_answer_t is_answer,
        void* userdata, int* out_errno)
{
    struct sockaddr_storage addr;
    socklen_t addr_length;
    int64_t deadline_us;
    ebisu_dns_asked_t asked;
    int flags;
    int sock;

    memset(&addr, 0, sizeof(addr));
    if (server->family == EBISU_DNS_IPV4) {
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
    sock = socket(server->family == EBISU_DNS_IPV4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        *out_errno = errno;
        return EBISU_DNS_ASKED_FAILED;
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
        return *out_errno == ECONNREFUSED ? EBISU_DNS_ASKED_REFUSED : EBISU_DNS_ASKED_FAILED;
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
            asked = EBISU_DNS_ASKED_TIMEOUT;
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
            asked = EBISU_DNS_ASKED_FAILED;
            break;
        }
        received = recv(sock, datagram, sizeof(datagram), 0);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue;
            }
            *out_errno = errno;
            asked = errno == ECONNREFUSED ? EBISU_DNS_ASKED_REFUSED : EBISU_DNS_ASKED_FAILED;
            break;
        }
        if (is_answer(datagram, (size_t)received, userdata)) {
            asked = EBISU_DNS_ASKED_ANSWERED;
            break;
        }
    }
    close(sock);
    return asked;
}

static size_t posix_servers(ebisu_dns_t* self, ebisu_dns_addr_t* out, size_t max)
{
    (void)self;
    return max == 0 ? 0 : _ebisu_posix_dns_read_servers(RESOLV_CONF, out, max,
            _EBISU_POSIX_DNS_READ_AS_GLIBC);
}

static ebisu_dns_asked_t posix_ask(ebisu_dns_t* self, const ebisu_dns_addr_t* server,
        const unsigned char* query, size_t length, uint32_t timeout_ms,
        ebisu_dns_is_answer_t is_answer, void* userdata, char* why, size_t why_size)
{
    int error = 0;
    ebisu_dns_asked_t asked = ask_once(server, query, length, timeout_ms, is_answer, userdata,
            &error);
    (void)self;
    if (asked == EBISU_DNS_ASKED_FAILED && why_size > 0) {
        snprintf(why, why_size, "%s", strerror(error));
    }
    return asked;
}

static uint32_t posix_scope_of(ebisu_dns_t* self, const char* name)
{
    (void)self;
    return (uint32_t)if_nametoindex(name);
}

static bool posix_scope_name(ebisu_dns_t* self, uint32_t scope_id, char* out, size_t size)
{
    char name[IF_NAMESIZE];
    (void)self;
    if (if_indextoname(scope_id, name) == NULL || strlen(name) >= size) {
        return false;
    }
    memcpy(out, name, strlen(name) + 1);
    return true;
}

static long posix_no_answer_error(ebisu_dns_t* self)
{
    (void)self;
    return EAI_AGAIN;
}

static const ebisu_dns_ops_t posix_dns_ops = {
    posix_servers,
    posix_ask,
    posix_scope_of,
    posix_scope_name,
    posix_no_answer_error
};

static ebisu_dns_t posix_dns = { &posix_dns_ops };

ebisu_dns_t* ebisu_posix_dns(void)
{
    return &posix_dns;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
