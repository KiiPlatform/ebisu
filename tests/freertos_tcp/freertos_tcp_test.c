/* The tests of the socket and the name servers over FreeRTOS-Plus-TCP, a
   firmware for the MPS2 AN385 board QEMU emulates, against servers of
   run_tcp_test.py's on the host, which QEMU's user-mode networking reaches at
   10.0.2.2. Their ports come on
   the command line, through semihosting. Each check that fails prints a line
   on the console, and the program ends with 1 if any did, which QEMU exits
   with. */
#include <ebisu_dns_probe.h>
#include <ebisu_freertos_tcp.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <FreeRTOS_DNS.h>
#include <FreeRTOS_IP.h>
#include <FreeRTOS_Routing.h>
#include <FreeRTOS_Sockets.h>

#include "board.h"

#define RUNNER_STACK 2048
#define RUNNER_PRIORITY (tskIDLE_PRIORITY + 2)

/* The host, as QEMU's user-mode networking has it. */
static const char host[] = "10.0.2.2";

/* The servers' ports, from the command line. */
static struct {
    uint16_t echo;
    uint16_t late;
    uint16_t partial;
    uint16_t closer;
    uint16_t reset;
    uint16_t unanswered;
    /* Name servers: one that answers, one that says the name doesn't exist,
       one that doesn't answer, one that sends an answer to another query
       first, one that sends only those, one that answers from another port,
       and a port nothing listens on. */
    uint16_t dns;
    uint16_t nxdomain;
    uint16_t silent;
    uint16_t stray;
    uint16_t strays;
    uint16_t elsewhere;
    uint16_t refused;
} ports;

static int checks;
static int failures;

static void print(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void print(const char* format, ...)
{
    char line[192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0) {
        length = 0;
    } else if ((size_t)length >= sizeof(line)) {
        length = sizeof(line) - 1;
    }
    board_write(line, (size_t)length);
    board_write("\r\n", 2);
}

static void check(bool ok, const char* what, int line)
{
    ++checks;
    if (!ok) {
        ++failures;
        print("FAILED at line %d: %s", line, what);
    }
}

static void check_compare(bool ok, const char* what, long long left, long long right, int line)
{
    ++checks;
    if (!ok) {
        ++failures;
        print("FAILED at line %d: %s, with %lld and %lld", line, what, left, right);
    }
}

/* Each side evaluated once: they may receive. */
#define CHECK(condition) check((condition), #condition, __LINE__)
#define CHECK_EQ(left, right) \
    do { \
        long long l_ = (long long)(left), r_ = (long long)(right); \
        check_compare(l_ == r_, #left " == " #right, l_, r_, __LINE__); \
    } while (0)
#define CHECK_LE(left, right) \
    do { \
        long long l_ = (long long)(left), r_ = (long long)(right); \
        check_compare(l_ <= r_, #left " <= " #right, l_, r_, __LINE__); \
    } while (0)

static uint32_t now_ms(void)
{
    return (uint32_t)((uint64_t)xTaskGetTickCount() * 1000u / configTICK_RATE_HZ);
}

/* The board's first CMSDK APB timer, counting down at the 25 MHz of its clock
   from 2^32 - 1 again and again, for how long a wait took in microseconds,
   which ticks can't tell: a wait of n ticks may be a little less than n of
   their periods. Its registers: control, value, reload. */
#define TIMER0 ((volatile uint32_t*)0x40000000u)
#define TIMER_HZ 25000000u

static void timer_start(void)
{
    TIMER0[0] = 0;
    TIMER0[2] = 0xffffffffu;
    TIMER0[1] = 0xffffffffu;
    TIMER0[0] = 1;
}

/* Cycles of the timer, up to 171 seconds' before they wrap. */
static uint32_t cycles(void)
{
    return 0xffffffffu - TIMER0[1];
}

static uint32_t us_since(uint32_t start)
{
    return (uint32_t)((uint64_t)(cycles() - start) * 1000000u / TIMER_HZ);
}

/* The command line QEMU is given with -semihosting-config arg=..., as
   name=value words: SYS_GET_CMDLINE. */
static int semihosting(int operation, const void* arguments)
{
    register int r0 __asm__("r0") = operation;
    register const void* r1 __asm__("r1") = arguments;
    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

static void read_ports(void)
{
    static char line[512];
    uintptr_t arguments[] = { (uintptr_t)line, sizeof(line) };
    if (semihosting(0x15, arguments) != 0) {
        board_fatal("no command line");
    }
    for (char* word = strtok(line, " "); word != NULL; word = strtok(NULL, " ")) {
        char* equals = strchr(word, '=');
        if (equals == NULL) {
            continue;
        }
        *equals = '\0';
        uint16_t port = (uint16_t)strtoul(equals + 1, NULL, 10);
        if (strcmp(word, "echo") == 0) {
            ports.echo = port;
        } else if (strcmp(word, "late") == 0) {
            ports.late = port;
        } else if (strcmp(word, "partial") == 0) {
            ports.partial = port;
        } else if (strcmp(word, "closer") == 0) {
            ports.closer = port;
        } else if (strcmp(word, "reset") == 0) {
            ports.reset = port;
        } else if (strcmp(word, "unanswered") == 0) {
            ports.unanswered = port;
        } else if (strcmp(word, "dns") == 0) {
            ports.dns = port;
        } else if (strcmp(word, "nxdomain") == 0) {
            ports.nxdomain = port;
        } else if (strcmp(word, "silent") == 0) {
            ports.silent = port;
        } else if (strcmp(word, "stray") == 0) {
            ports.stray = port;
        } else if (strcmp(word, "strays") == 0) {
            ports.strays = port;
        } else if (strcmp(word, "elsewhere") == 0) {
            ports.elsewhere = port;
        } else if (strcmp(word, "refused") == 0) {
            ports.refused = port;
        }
    }
    if (ports.echo == 0 || ports.late == 0 || ports.partial == 0 || ports.closer == 0 ||
            ports.reset == 0 || ports.unanswered == 0 || ports.dns == 0 ||
            ports.nxdomain == 0 || ports.silent == 0 || ports.stray == 0 ||
            ports.strays == 0 || ports.elsewhere == 0 || ports.refused == 0) {
        board_fatal("a server's port is missing from the command line");
    }
}

/* A logger that keeps the last line it was given. */
static struct {
    ebisu_logger_t base;
    char last[160];
    int lines;
} captured;

static void capture(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args)
{
    (void)self;
    (void)level;
    vsnprintf(captured.last, sizeof(captured.last), fmt, args);
    ++captured.lines;
}

static const ebisu_logger_ops_t capture_ops = { capture };

static ebisu_sock_error_t last_error(ebisu_freertos_tcp_socket_t* sock, long* detail)
{
    long ignored = 0;
    return sock->base.ops->last_error(&sock->base, detail != NULL ? detail : &ignored);
}

static bool send_all(ebisu_freertos_tcp_socket_t* sock, const char* data)
{
    size_t length = strlen(data);
    while (length > 0) {
        size_t sent = 0;
        if (sock->base.ops->send(&sock->base, data, length, &sent) != EBISU_SOCK_OK) {
            return false;
        }
        data += sent;
        length -= sent;
    }
    return true;
}

/* Receives until length bytes or the end of the stream. */
static size_t recv_all(ebisu_freertos_tcp_socket_t* sock, char* buffer, size_t length)
{
    size_t got = 0;
    while (got < length) {
        size_t received = 0;
        if (sock->base.ops->recv(&sock->base, buffer + got, length - got, &received) !=
                EBISU_SOCK_OK || received == 0) {
            break;
        }
        got += received;
    }
    buffer[got] = '\0';
    return got;
}

static void test_echo(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    /* Twice, the socket used again once closed. */
    for (int i = 0; i < 2; ++i) {
        CHECK(sock.base.ops->connect(&sock.base, host, ports.echo) == EBISU_SOCK_OK);
        CHECK(send_all(&sock, "hello"));
        CHECK_EQ(recv_all(&sock, buffer, 5), 5);
        CHECK(strcmp(buffer, "hello") == 0);
        CHECK(sock.base.ops->close(&sock.base) == EBISU_SOCK_OK);
    }
    CHECK(last_error(&sock, NULL) == EBISU_SOCK_ERROR_NONE);
}

/* By a name, which FreeRTOS_getaddrinfo() looks up: in FreeRTOS-Plus-TCP's DNS
   cache, given the host's address here, without a server. */
static void test_by_name(void)
{
    ebisu_freertos_tcp_socket_t sock;
    IPv46_Address_t address;
    char buffer[16];
    memset(&address, 0, sizeof(address));
    address.xIPAddress.ulIP_IPv4 = FreeRTOS_inet_addr(host);
    address.xIs_IPv6 = pdFALSE;
    (void)FreeRTOS_dns_update("echo.test", &address, 3600, pdFALSE, NULL);
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    CHECK(sock.base.ops->connect(&sock.base, "echo.test", ports.echo) == EBISU_SOCK_OK);
    CHECK(send_all(&sock, "named"));
    CHECK_EQ(recv_all(&sock, buffer, 5), 5);
    CHECK(strcmp(buffer, "named") == 0);
    sock.base.ops->close(&sock.base);
}

/* The server sends 3 seconds after the connection. */
static void test_recv_timeout(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    size_t received = 1;
    captured.lines = 0;
    ebisu_freertos_tcp_socket_init(&sock, 1, 5);
    ebisu_freertos_tcp_socket_set_logger(&sock, &captured.base);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.late) == EBISU_SOCK_OK);
    uint32_t start = now_ms();
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) ==
            EBISU_SOCK_FAIL);
    uint32_t took = now_ms() - start;
    CHECK_EQ(received, 0);
    CHECK(sock.base.ops->recv_timed_out(&sock.base));
    CHECK(last_error(&sock, NULL) == EBISU_SOCK_ERROR_TIMEOUT);
    CHECK_LE(900, took);
    CHECK_LE(took, 2500);
    /* Not a failure to log. */
    CHECK_EQ(captured.lines, 0);
    /* The connection goes on, and the data comes. */
    ebisu_sock_code_t code = EBISU_SOCK_FAIL;
    for (int i = 0; i < 5 && code != EBISU_SOCK_OK; ++i) {
        code = sock.base.ops->recv(&sock.base, buffer, sizeof(buffer) - 1, &received);
    }
    CHECK(code == EBISU_SOCK_OK);
    buffer[code == EBISU_SOCK_OK ? received : 0] = '\0';
    CHECK(strcmp(buffer, "late") == 0);
    CHECK(!sock.base.ops->recv_timed_out(&sock.base));
    sock.base.ops->close(&sock.base);
}

static void test_recv_wait(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    size_t received = 0;
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    CHECK(sock.base.ops->recv_wait != NULL);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.late) == EBISU_SOCK_OK);
    /* Nothing for its timeout. */
    uint32_t start = now_ms();
    CHECK(!sock.base.ops->recv_wait(&sock.base, 500));
    uint32_t took = now_ms() - start;
    CHECK_LE(450, took);
    CHECK_LE(took, 1500);
    /* The receive timeout as it was, 5 seconds: recv waits for the data,
       which comes 3 seconds after the connection. */
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer) - 1, &received) ==
            EBISU_SOCK_OK);
    buffer[received] = '\0';
    CHECK(strcmp(buffer, "late") == 0);
    sock.base.ops->close(&sock.base);

    /* What recv hasn't returned yet, at once. */
    CHECK(sock.base.ops->connect(&sock.base, host, ports.partial) == EBISU_SOCK_OK);
    CHECK(sock.base.ops->recv_wait(&sock.base, 10000));
    CHECK(sock.base.ops->recv(&sock.base, buffer, 2, &received) == EBISU_SOCK_OK);
    CHECK_EQ(received, 2);
    start = now_ms();
    CHECK(sock.base.ops->recv_wait(&sock.base, 5000));
    CHECK_LE(now_ms() - start, 200);
    CHECK_EQ(recv_all(&sock, buffer, 4), 4);
    CHECK(strcmp(buffer, "cdef") == 0);
    sock.base.ops->close(&sock.base);
}

/* The server sends "bye", and closes. */
static void test_end_of_stream(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    size_t received = 1;
    size_t sent = 1;
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    captured.lines = 0;
    ebisu_freertos_tcp_socket_set_logger(&sock, &captured.base);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.closer) == EBISU_SOCK_OK);
    CHECK_EQ(recv_all(&sock, buffer, 3), 3);
    CHECK(strcmp(buffer, "bye") == 0);
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK);
    CHECK_EQ(received, 0);
    /* The peer gone, recv_wait doesn't wait. */
    CHECK(sock.base.ops->recv_wait(&sock.base, 5000));
    /* A send, as a TLS socket's close_notify, fails as closed, as
       FreeRTOS-Plus-TCP sends nothing once the peer has closed its side, and
       nothing is logged. */
    CHECK(sock.base.ops->send(&sock.base, "data", 4, &sent) == EBISU_SOCK_FAIL);
    CHECK_EQ(sent, 0);
    CHECK(last_error(&sock, NULL) == EBISU_SOCK_ERROR_CLOSED);
    CHECK_EQ(captured.lines, 0);
    sock.base.ops->close(&sock.base);
}

/* The server resets the connection. */
static void test_reset(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    size_t received = 1;
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    captured.lines = 0;
    ebisu_freertos_tcp_socket_set_logger(&sock, &captured.base);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.reset) == EBISU_SOCK_OK);
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK);
    CHECK_EQ(received, 0);
    size_t sent = 0;
    CHECK(sock.base.ops->send(&sock.base, "data", 4, &sent) == EBISU_SOCK_FAIL);
    CHECK(last_error(&sock, NULL) == EBISU_SOCK_ERROR_CLOSED);
    /* Reset, the connection's failure is said. */
    CHECK_EQ(captured.lines, 1);
    CHECK(strstr(captured.last, "failed to send: ") != NULL);
    sock.base.ops->close(&sock.base);
}

/* The end-points, the first one's address from DHCP, as QEMU's user-mode
   networking gives it, with its name server, 10.0.2.3. The second is never
   up: it has name servers of its own, for those of the first to lead to. */
static NetworkInterface_t interface;
static NetworkEndPoint_t end_point;
static NetworkEndPoint_t second_end_point;

static void test_connect_failures(void)
{
    ebisu_freertos_tcp_socket_t sock;
    long detail = 0;
    captured.lines = 0;
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    ebisu_freertos_tcp_socket_set_logger(&sock, &captured.base);

    /* A name server that never answers, for what an address as written
       mustn't be, a name to look up, to take long: each of the DNS client's
       attempts waits 5 seconds. */
    const uint32_t server = end_point.ipv4_settings.ulDNSServerAddresses[0];
    end_point.ipv4_settings.ulDNSServerAddresses[0] = FreeRTOS_inet_addr("192.0.2.1");

    /* A port nothing listens on, which QEMU's user-mode networking leaves
       unanswered rather than refuse: the connect timeout. */
    ebisu_freertos_tcp_socket_set_connect_timeout(&sock, 2);
    uint32_t start = now_ms();
    CHECK(sock.base.ops->connect(&sock.base, host, ports.unanswered) == EBISU_SOCK_FAIL);
    uint32_t took = now_ms() - start;
    CHECK(last_error(&sock, &detail) == EBISU_SOCK_ERROR_TIMEOUT);
    CHECK_EQ(detail, pdFREERTOS_ERRNO_ETIMEDOUT);
    CHECK_LE(1900, took);
    CHECK_LE(took, 4000);
#ifndef EBISU_NO_LOG
    CHECK_EQ(captured.lines, 1);
    CHECK(strstr(captured.last, "failed to connect to 10.0.2.2:") != NULL);
#endif
#if ipconfigUSE_IPv6 != 0
    /* An IPv6 address as written, which the network, IPv4 only, has no route
       to, isn't looked up for IPv4 first either: with LLMNR, which
       FreeRTOS-Plus-TCP asks for a name without a dot, as it is. */
    start = now_ms();
    CHECK(sock.base.ops->connect(&sock.base, "fec0::2", ports.unanswered) == EBISU_SOCK_FAIL);
    took = now_ms() - start;
    CHECK(last_error(&sock, &detail) != EBISU_SOCK_ERROR_DNS);
    CHECK_LE(took, 4000);
#endif
    end_point.ipv4_settings.ulDNSServerAddresses[0] = server;

    /* A host that doesn't resolve, as QEMU's DNS server forwards it to the
       host's resolver. */
    CHECK(sock.base.ops->connect(&sock.base, "no-such-host.invalid", 443) == EBISU_SOCK_FAIL);
    CHECK(last_error(&sock, &detail) == EBISU_SOCK_ERROR_DNS);
    CHECK_EQ(detail, pdFREERTOS_ERRNO_ENOENT);
#ifndef EBISU_NO_LOG
    CHECK(strstr(captured.last, "failed to look up no-such-host.invalid") != NULL);
#endif
}

/* What the DNS probe's own query has, which a name server's answer repeats:
   an identifier, and a question, here for test.'s IPv4 addresses. */
static size_t make_query(uint16_t id, unsigned char* out)
{
    static const unsigned char rest[] = {
        0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        4, 't', 'e', 's', 't', 0, 0x00, 0x01, 0x00, 0x01
    };
    out[0] = (unsigned char)(id >> 8);
    out[1] = (unsigned char)(id & 0xff);
    memcpy(out + 2, rest, sizeof(rest));
    return 2 + sizeof(rest);
}

/* Counts the datagrams ask gives it, and takes the answer to the query of the
   identifier given. */
static struct {
    uint16_t id;
    int datagrams;
    /* How long the last ask took, in microseconds. */
    uint32_t took_us;
} answer;

static bool is_answer(const unsigned char* datagram, size_t length, void* userdata)
{
    CHECK(userdata == &answer);
    ++answer.datagrams;
    return length >= 12 && datagram[0] == (unsigned char)(answer.id >> 8) &&
        datagram[1] == (unsigned char)(answer.id & 0xff) && (datagram[2] & 0x80) != 0;
}

static ebisu_dns_addr_t ipv4_server(const char* address, uint16_t port)
{
    ebisu_dns_addr_t server;
    CHECK(ebisu_dns_parse_address(NULL, address, strlen(address), port, false, &server));
    return server;
}

/* Asks the name server on the host's port, and says how long it took. */
static ebisu_dns_asked_t ask(uint16_t port, uint32_t timeout_ms, uint32_t* took, char* why,
        size_t why_size)
{
    ebisu_dns_t* dns = ebisu_freertos_tcp_dns();
    ebisu_dns_addr_t server = ipv4_server(host, port);
    unsigned char query[64];
    size_t length;
    ++answer.id;
    answer.datagrams = 0;
    length = make_query(answer.id, query);
    why[0] = '\0';
    uint32_t start = now_ms();
    uint32_t start_cycles = cycles();
    ebisu_dns_asked_t asked = dns->ops->ask(dns, &server, query, length, timeout_ms,
            is_answer, &answer, why, why_size);
    answer.took_us = us_since(start_cycles);
    *took = now_ms() - start;
    return asked;
}

static void test_dns_ask(void)
{
    char why[128];
    uint32_t took = 0;

    /* An answer, and one that says the name doesn't exist, which is one too. */
    CHECK(ask(ports.dns, 3000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED);
    CHECK_EQ(answer.datagrams, 1);
    CHECK_LE(took, 1000);
    CHECK(ask(ports.nxdomain, 3000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED);
    CHECK_EQ(answer.datagrams, 1);

    /* The answer to another query skipped, and the wait gone on. */
    CHECK(ask(ports.stray, 3000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED);
    CHECK_EQ(answer.datagrams, 2);
    CHECK_LE(250, took);
    CHECK_LE(took, 1500);

    /* None in time: the whole of it waited, whatever comes meanwhile. */
    CHECK(ask(ports.strays, 1000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT);
    CHECK_LE(4, answer.datagrams);
    CHECK_LE(1000, took);
    CHECK_LE(1000000, answer.took_us);
    CHECK_LE(took, 1500);
    CHECK(ask(ports.silent, 1000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT);
    CHECK_EQ(answer.datagrams, 0);
    CHECK_LE(1000, took);
    CHECK_LE(1000000, answer.took_us);
    CHECK_LE(took, 1500);
    /* An answer from another port isn't the server's. */
    CHECK(ask(ports.elsewhere, 1000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT);
    CHECK_EQ(answer.datagrams, 0);
    CHECK_LE(1000, took);
    CHECK_LE(took, 1500);
    /* Short ones as long as they say, the tick under way counted as none. */
    for (uint32_t ms = 1; ms <= 5; ++ms) {
        CHECK(ask(ports.silent, ms, &took, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT);
        CHECK_LE(ms * 1000u, answer.took_us);
        CHECK_LE(took, 100);
    }

    /* Where nothing listens, the host refuses, but ICMP's port unreachable
       doesn't reach the socket: as one that doesn't answer. */
    CHECK(ask(ports.refused, 1000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_TIMEOUT);
    CHECK_LE(1000, took);

    /* The socket used for each ask goes, and another comes, many times over. */
    for (int i = 0; i < 40; ++i) {
        CHECK(ask(ports.dns, 3000, &took, why, sizeof(why)) == EBISU_DNS_ASKED_ANSWERED);
    }

#if ipconfigUSE_IPv6 == 0
    /* An IPv6 server, without IPv6. */
    {
        ebisu_dns_t* dns = ebisu_freertos_tcp_dns();
        ebisu_dns_addr_t server;
        unsigned char query[64];
        size_t length = make_query(++answer.id, query);
        CHECK(ebisu_dns_parse_address(NULL, "::1", 3, 53, false, &server));
        CHECK(dns->ops->ask(dns, &server, query, length, 1000, is_answer, &answer, why,
                    sizeof(why)) == EBISU_DNS_ASKED_FAILED);
        CHECK(strcmp(why, "FreeRTOS-Plus-TCP is built without IPv6") == 0);
    }
#endif
}

/* The servers, as ebisu_freertos_tcp_dns() has them, as text, each followed by
   a space, as many as max. */
static const char* servers_of(size_t max)
{
    static char text[256];
    ebisu_dns_t* dns = ebisu_freertos_tcp_dns();
    ebisu_dns_addr_t servers[8];
    size_t used = 0;
    size_t count = dns->ops->servers(dns, servers, max);
    text[0] = '\0';
    CHECK_LE(count, max);
    for (size_t i = 0; i < count && i < 8; ++i) {
        char address[EBISU_DNS_ADDRESS_TEXT_SIZE];
        ebisu_dns_format_address(dns, &servers[i], address, sizeof(address));
        used += (size_t)snprintf(text + used, sizeof(text) - used, "%s ", address);
    }
    return text;
}

/* Sets the end-point's servers, by their addresses as written, "" for none,
   and the index of the one the next lookup asks. */
static void set_servers(NetworkEndPoint_t* point, const char* first, const char* second,
        const char* third, uint8_t index)
{
    const char* addresses[3] = { first, second, third };
    vTaskSuspendAll();
    for (size_t i = 0; i < ipconfigENDPOINT_DNS_ADDRESS_COUNT; ++i) {
        point->ipv4_settings.ulDNSServerAddresses[i] =
            addresses[i][0] == '\0' ? 0 : FreeRTOS_inet_addr(addresses[i]);
    }
    point->ipv4_settings.ucDNSIndex = index;
    (void)xTaskResumeAll();
}

#define CHECK_SERVERS(max, expected) \
    do { \
        const char* text_ = servers_of(max); \
        if (strcmp(text_, expected) != 0) { \
            print("servers: \"%s\", not \"%s\"", text_, expected); \
        } \
        CHECK(strcmp(text_, expected) == 0); \
    } while (0)

static void test_dns_servers(void)
{
    IPV4Parameters_t settings = end_point.ipv4_settings;

    /* As DHCP gave them: QEMU's server, on its own. */
    CHECK_SERVERS(8, "10.0.2.3:53 ");
    CHECK_SERVERS(0, "");

    /* From the one the next lookup asks, then each other in turn. */
    set_servers(&end_point, "192.0.2.1", "192.0.2.2", "192.0.2.3", 0);
    CHECK_SERVERS(8, "192.0.2.1:53 192.0.2.2:53 192.0.2.3:53 ");
    CHECK_SERVERS(2, "192.0.2.1:53 192.0.2.2:53 ");
    set_servers(&end_point, "192.0.2.1", "192.0.2.2", "192.0.2.3", 2);
    CHECK_SERVERS(8, "192.0.2.3:53 192.0.2.1:53 192.0.2.2:53 ");
    /* None skipped. */
    set_servers(&end_point, "192.0.2.1", "", "192.0.2.3", 2);
    CHECK_SERVERS(8, "192.0.2.3:53 192.0.2.1:53 ");
    set_servers(&end_point, "192.0.2.1", "", "", 0);
    CHECK_SERVERS(8, "192.0.2.1:53 ");

    /* The client goes on to the second end-point where the first has none for
       the next lookup, and stays with it, its index unmoved. */
    set_servers(&end_point, "", "192.0.2.2", "", 0);
    CHECK_SERVERS(8, "198.51.100.1:53 198.51.100.2:53 ");
    /* And after the broadcast address, which it takes for none. */
    set_servers(&end_point, "192.0.2.1", "255.255.255.255", "192.0.2.3", 0);
    CHECK_SERVERS(8, "192.0.2.1:53 198.51.100.1:53 198.51.100.2:53 ");
    set_servers(&second_end_point, "198.51.100.1", "198.51.100.2", "", 1);
    CHECK_SERVERS(8, "192.0.2.1:53 198.51.100.2:53 198.51.100.1:53 ");
    /* An index out of the end-point's: none of it. */
    set_servers(&end_point, "192.0.2.1", "", "", ipconfigENDPOINT_DNS_ADDRESS_COUNT);
    CHECK_SERVERS(8, "198.51.100.2:53 198.51.100.1:53 ");
    set_servers(&second_end_point, "", "", "", 0);
    CHECK_SERVERS(8, "");

    /* Only IPv6 ones, where the client prefers them. */
#if ipconfigUSE_IPv6 != 0
    set_servers(&end_point, "192.0.2.1", "", "", 0);
    CHECK(FreeRTOS_SetDNSIPPreference(xPreferenceIPv6) == pdPASS);
    CHECK_SERVERS(8, "");
    CHECK(FreeRTOS_SetDNSIPPreference(xPreferenceIPv4) == pdPASS);
#endif

    vTaskSuspendAll();
    end_point.ipv4_settings = settings;
    (void)xTaskResumeAll();
    set_servers(&second_end_point, "198.51.100.1", "198.51.100.2", "", 0);
    CHECK_SERVERS(8, "10.0.2.3:53 ");
}

static void test_dns_probe(void)
{
    ebisu_dns_t* dns = ebisu_freertos_tcp_dns();
    char detail[EBISU_DNS_PROBE_DETAIL_SIZE];

    /* The system's: QEMU's server, which passes the query to the host's
       resolver, which answers that the name doesn't exist. */
    CHECK(ebisu_dns_probe(dns, "no-such-host.invalid", 5000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_ANSWERED);
    CHECK(strcmp(detail, "10.0.2.3:53 answered") == 0);
    CHECK(ebisu_dns_probe(dns, host, 5000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_NOT_NEEDED);

    /* Servers given: each in turn, until one answers. */
    char silent[8];
    char dns_port[8];
    snprintf(silent, sizeof(silent), "%u", (unsigned int)ports.silent);
    snprintf(dns_port, sizeof(dns_port), "%u", (unsigned int)ports.dns);
    const ebisu_dns_server_t two[] = { { host, ports.silent }, { host, ports.dns } };
    uint32_t start = now_ms();
    CHECK(ebisu_dns_probe_servers(dns, "example.com", two, 2, 1000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_ANSWERED);
    CHECK_LE(1000, now_ms() - start);
    CHECK(strstr(detail, dns_port) != NULL && strstr(detail, " answered") != NULL);
    CHECK(ebisu_dns_probe_servers(dns, "example.com", two, 1, 1000, detail, sizeof(detail)) ==
            EBISU_DNS_PROBE_SILENT);
    CHECK(strstr(detail, silent) != NULL && strstr(detail, " didn't answer in time") != NULL);
}

/* The probe's socket over the TCP one: no connect where no server answers,
   and one where one does. */
static void test_dns_probe_socket(void)
{
    static ebisu_dns_server_t silent[1];
    static ebisu_dns_server_t answering[1];
    ebisu_freertos_tcp_socket_t inner;
    ebisu_dns_probe_socket_t sock;
    char buffer[16];
    long detail = 0;
    silent[0].address = host;
    silent[0].port = ports.silent;
    answering[0].address = host;
    answering[0].port = ports.dns;
    ebisu_freertos_tcp_socket_init(&inner, 5, 5);
    ebisu_dns_probe_socket_init(&sock, &inner.base, ebisu_freertos_tcp_dns(), 1000);

    ebisu_dns_probe_socket_set_servers(&sock, silent, 1);
    uint32_t start = now_ms();
    CHECK(sock.base.ops->connect(&sock.base, "echo.test", ports.echo) == EBISU_SOCK_FAIL);
    uint32_t took = now_ms() - start;
    CHECK(sock.base.ops->last_error(&sock.base, &detail) == EBISU_SOCK_ERROR_DNS);
    CHECK_EQ(detail, pdFREERTOS_ERRNO_ENOENT);
    CHECK_LE(1000, took);
    CHECK_LE(took, 1500);
    CHECK(inner._socket == FREERTOS_INVALID_SOCKET);
    CHECK(sock.base.ops->close(&sock.base) == EBISU_SOCK_OK);

    /* echo.test from FreeRTOS-Plus-TCP's DNS cache, as test_by_name() put it
       there. */
    ebisu_dns_probe_socket_set_servers(&sock, answering, 1);
    CHECK(sock.base.ops->connect(&sock.base, "echo.test", ports.echo) == EBISU_SOCK_OK);
    CHECK(send_all(&inner, "probed"));
    CHECK_EQ(recv_all(&inner, buffer, 6), 6);
    CHECK(strcmp(buffer, "probed") == 0);
    CHECK(sock.base.ops->close(&sock.base) == EBISU_SOCK_OK);
}

static SemaphoreHandle_t network_up;

static void runner(void* arg)
{
    (void)arg;
    read_ports();
    timer_start();
    captured.base.ops = &capture_ops;
    xSemaphoreTake(network_up, portMAX_DELAY);
    print("ebisu FreeRTOS-Plus-TCP socket and name server tests: FreeRTOS %s, "
            "FreeRTOS-Plus-TCP %s",
            tskKERNEL_VERSION_NUMBER, ipFR_TCP_VERSION_NUMBER);
    test_echo();
    test_by_name();
    test_recv_timeout();
    test_recv_wait();
    test_end_of_stream();
    test_reset();
    test_connect_failures();
    test_dns_ask();
    test_dns_servers();
    test_dns_probe();
    test_dns_probe_socket();
    print("%d checks, %d failed", checks, failures);
    board_exit(failures == 0 ? 0 : 1);
}

/* From FreeRTOS-Plus-TCP's driver for the LAN9118, which has no header. */
NetworkInterface_t* pxMPS2_FillInterfaceDescriptor(BaseType_t xEMACIndex,
        NetworkInterface_t* pxInterface);

static const uint8_t mac_address[ipMAC_ADDRESS_LENGTH_BYTES] = {
    0x52, 0x54, 0x00, 0x12, 0x34, 0x56
};

int main(void)
{
    static const uint8_t none[ipIP_ADDRESS_LENGTH_BYTES] = { 0 };
    board_init();
    network_up = xSemaphoreCreateBinary();
    configASSERT(network_up != NULL);
    pxMPS2_FillInterfaceDescriptor(0, &interface);
    FreeRTOS_FillEndPoint(&interface, &end_point, none, none, none, none, mac_address);
    end_point.bits.bWantDHCP = pdTRUE;
    {
        static const uint8_t address[] = { 192, 168, 77, 2 };
        static const uint8_t mask[] = { 255, 255, 255, 0 };
        static const uint8_t dns[] = { 198, 51, 100, 1 };
        FreeRTOS_FillEndPoint(&interface, &second_end_point, address, mask, none, dns,
                mac_address);
        second_end_point.ipv4_settings.ulDNSServerAddresses[1] =
            FreeRTOS_inet_addr("198.51.100.2");
        second_end_point.ipv4_defaults.ulDNSServerAddresses[1] =
            second_end_point.ipv4_settings.ulDNSServerAddresses[1];
    }
    if (FreeRTOS_IPInit_Multi() != pdPASS) {
        board_fatal("failed to start the network");
    }
    if (xTaskCreate(runner, "runner", RUNNER_STACK, NULL, RUNNER_PRIORITY, NULL) != pdPASS) {
        board_fatal("failed to make the runner task");
    }
    vTaskStartScheduler();
    board_fatal("failed to start the scheduler");
}

/* FreeRTOS's and FreeRTOS-Plus-TCP's hooks. */

void vApplicationIPNetworkEventHook_Multi(eIPCallbackEvent_t eNetworkEvent,
        struct xNetworkEndPoint* pxEndPoint)
{
    /* The first's, whose address DHCP gives. */
    if (eNetworkEvent == eNetworkUp && pxEndPoint == &end_point) {
        xSemaphoreGive(network_up);
    }
}

#if ipconfigUSE_LLMNR != 0
/* No name LLMNR asks of is the board's. */
BaseType_t xApplicationDNSQueryHook_Multi(struct xNetworkEndPoint* pxEndPoint, const char* pcName)
{
    (void)pxEndPoint;
    (void)pcName;
    return pdFALSE;
}
#endif

BaseType_t xApplicationGetRandomNumber(uint32_t* pulNumber)
{
    return board_entropy((unsigned char*)pulNumber, sizeof(*pulNumber)) == 0 ? pdTRUE : pdFALSE;
}

uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
        uint16_t usSourcePort, uint32_t ulDestinationAddress,
        uint16_t usDestinationPort)
{
    (void)ulSourceAddress;
    (void)usSourcePort;
    (void)ulDestinationAddress;
    (void)usDestinationPort;
    uint32_t number = 0;
    return board_entropy((unsigned char*)&number, sizeof(number)) == 0 ? number : 0;
}

void vApplicationMallocFailedHook(void)
{
    board_fatal("out of memory");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    board_fatal("stack overflow in task %s", name);
}
