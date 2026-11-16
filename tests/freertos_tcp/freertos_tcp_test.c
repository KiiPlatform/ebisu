/* The tests of the socket over FreeRTOS-Plus-TCP, a firmware for the MPS2
   AN385 board QEMU emulates, against servers of run_tcp_test.py's on the host,
   which QEMU's user-mode networking reaches at 10.0.2.2. Their ports come on
   the command line, through semihosting. Each check that fails prints a line
   on the console, and the program ends with 1 if any did, which QEMU exits
   with. */
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
    static char line[256];
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
        }
    }
    if (ports.echo == 0 || ports.late == 0 || ports.partial == 0 || ports.closer == 0 ||
            ports.reset == 0 || ports.unanswered == 0) {
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
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.closer) == EBISU_SOCK_OK);
    CHECK_EQ(recv_all(&sock, buffer, 3), 3);
    CHECK(strcmp(buffer, "bye") == 0);
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK);
    CHECK_EQ(received, 0);
    /* The peer gone, recv_wait doesn't wait. */
    CHECK(sock.base.ops->recv_wait(&sock.base, 5000));
    sock.base.ops->close(&sock.base);
}

/* The server resets the connection. */
static void test_reset(void)
{
    ebisu_freertos_tcp_socket_t sock;
    char buffer[16];
    size_t received = 1;
    ebisu_freertos_tcp_socket_init(&sock, 5, 5);
    CHECK(sock.base.ops->connect(&sock.base, host, ports.reset) == EBISU_SOCK_OK);
    CHECK(sock.base.ops->recv(&sock.base, buffer, sizeof(buffer), &received) == EBISU_SOCK_OK);
    CHECK_EQ(received, 0);
    size_t sent = 0;
    CHECK(sock.base.ops->send(&sock.base, "data", 4, &sent) == EBISU_SOCK_FAIL);
    CHECK(last_error(&sock, NULL) == EBISU_SOCK_ERROR_CLOSED);
    sock.base.ops->close(&sock.base);
}

/* The end-point, whose address DHCP gives, as QEMU's user-mode networking
   has it, with its name server, 10.0.2.3. */
static NetworkInterface_t interface;
static NetworkEndPoint_t end_point;

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

static SemaphoreHandle_t network_up;

static void runner(void* arg)
{
    (void)arg;
    read_ports();
    captured.base.ops = &capture_ops;
    xSemaphoreTake(network_up, portMAX_DELAY);
    print("ebisu FreeRTOS-Plus-TCP socket tests: FreeRTOS %s, FreeRTOS-Plus-TCP %s",
            tskKERNEL_VERSION_NUMBER, ipFR_TCP_VERSION_NUMBER);
    test_echo();
    test_by_name();
    test_recv_timeout();
    test_recv_wait();
    test_end_of_stream();
    test_reset();
    test_connect_failures();
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
    (void)pxEndPoint;
    if (eNetworkEvent == eNetworkUp) {
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
