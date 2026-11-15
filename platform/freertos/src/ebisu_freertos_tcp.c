#include "ebisu_freertos_tcp.h"

#include <ebisu_object.h>

#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <FreeRTOS_DNS.h>
#include <FreeRTOS_IP.h>
#include <FreeRTOS_Sockets.h>

#if ipconfigUSE_TCP != 1
#error "ebisu_freertos_tcp_socket_t is TCP: set ipconfigUSE_TCP to 1."
#endif
#if ipconfigUSE_DNS != 1
#error "ebisu_freertos_tcp_socket_t looks hosts up: set ipconfigUSE_DNS to 1."
#endif

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

/* How long the peer has to close its side once this one has. */
#define CLOSE_WAIT_MS 2000u
/* How long each receive of the close's waits, in turns of it. */
#define CLOSE_TURN_MS 250u

/* Ticks of milliseconds, of at most what a TickType_t holds short of
   portMAX_DELAY, which is for ever with INCLUDE_vTaskSuspend. */
static TickType_t ms_to_ticks(uint64_t msec)
{
    uint64_t ticks = (msec * configTICK_RATE_HZ + 999u) / 1000u;
    return ticks >= portMAX_DELAY ? (TickType_t)(portMAX_DELAY - 1) : (TickType_t)ticks;
}

/* A timeout in seconds as a socket's: 0 for none, as portMAX_DELAY. */
static TickType_t timeout_ticks(unsigned int seconds)
{
    return seconds == 0 ? portMAX_DELAY : ms_to_ticks((uint64_t)seconds * 1000u);
}

static void set_timeouts(Socket_t socket, TickType_t to_recv, TickType_t to_send)
{
    (void)FreeRTOS_setsockopt(socket, 0, FREERTOS_SO_RCVTIMEO, &to_recv, sizeof(to_recv));
    (void)FreeRTOS_setsockopt(socket, 0, FREERTOS_SO_SNDTIMEO, &to_send, sizeof(to_send));
}

static ebisu_sock_code_t fail_with(ebisu_freertos_tcp_socket_t* sock, ebisu_sock_error_t error,
        long detail)
{
    sock->_error = error;
    sock->_error_detail = detail;
    return EBISU_SOCK_FAIL;
}

/* Connects to an address, whose port is set here. Whether it took the
   connection; otherwise why not is the socket's error. */
static bool connect_to(ebisu_freertos_tcp_socket_t* sock, struct freertos_sockaddr server,
        uint16_t port)
{
    const TickType_t to_connect = timeout_ticks(sock->_to_connect);
    Socket_t socket = FreeRTOS_socket(server.sin_family, FREERTOS_SOCK_STREAM,
            FREERTOS_IPPROTO_TCP);
    BaseType_t ret;
    if (socket == FREERTOS_INVALID_SOCKET) {
        fail_with(sock, EBISU_SOCK_ERROR_OTHER, 0);
        return false;
    }
    /* connect waits for as long as a receive does. */
    set_timeouts(socket, to_connect, to_connect);
    server.sin_port = FreeRTOS_htons(port);
    ret = FreeRTOS_connect(socket, &server, sizeof(server));
    if (ret == 0) {
        set_timeouts(socket, timeout_ticks(sock->_to_recv), timeout_ticks(sock->_to_send));
        sock->_socket = socket;
        return true;
    }
    /* A refusal and a reset are both -pdFREERTOS_ERRNO_ENOTCONN. */
    fail_with(sock, ret == -pdFREERTOS_ERRNO_ETIMEDOUT ?
            EBISU_SOCK_ERROR_TIMEOUT : EBISU_SOCK_ERROR_CONNECT, (long)-ret);
    (void)FreeRTOS_closesocket(socket);
    return false;
}

/* The host as an address of the family as written, which FreeRTOS_getaddrinfo()
   takes so only with ipconfigINCLUDE_FULL_INET_ADDR, which isn't set by
   default. Whether it is one. */
static bool read_address(const char* host, BaseType_t family, struct freertos_sockaddr* out)
{
    memset(out, 0, sizeof(*out));
    out->sin_len = (uint8_t)sizeof(*out);
    out->sin_family = (uint8_t)family;
#if ipconfigUSE_IPv6 != 0
    if (family == FREERTOS_AF_INET6) {
        return FreeRTOS_inet_pton(family, host, out->sin_address.xIP_IPv6.ucBytes) == pdPASS;
    }
#endif
    return FreeRTOS_inet_pton(family, host, &out->sin_address.ulIP_IPv4) == pdPASS;
}

/* Connects to the host, an address of the family or a name with addresses of
   it, each in turn until one takes the connection. Whether one did; otherwise
   why the last failed is the socket's error. */
static bool connect_family(ebisu_freertos_tcp_socket_t* sock, const char* host, uint16_t port,
        BaseType_t family)
{
    struct freertos_addrinfo hints;
    struct freertos_addrinfo* results = NULL;
    struct freertos_addrinfo* address;
    struct freertos_sockaddr written;
    BaseType_t ret;

    if (read_address(host, family, &written)) {
        return connect_to(sock, written, port);
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    ret = FreeRTOS_getaddrinfo(host, NULL, &hints, &results);
    if (ret != 0 || results == NULL) {
        fail_with(sock, EBISU_SOCK_ERROR_DNS, (long)-ret);
        return false;
    }
    for (address = results; address != NULL; address = address->ai_next) {
        /* FreeRTOS_getaddrinfo() gives the family in ai_family, and not in the
           address, which FreeRTOS_socket() takes it from. */
        struct freertos_sockaddr server = *address->ai_addr;
        server.sin_len = (uint8_t)sizeof(server);
        server.sin_family = (uint8_t)address->ai_family;
        if (connect_to(sock, server, port)) {
            FreeRTOS_freeaddrinfo(results);
            return true;
        }
    }
    FreeRTOS_freeaddrinfo(results);
    return false;
}

static ebisu_sock_code_t tcp_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    sock->_error = EBISU_SOCK_ERROR_NONE;
    sock->_error_detail = 0;
    sock->_recv_timed_out = false;
    if (connect_family(sock, host, port, FREERTOS_AF_INET4)) {
        return EBISU_SOCK_OK;
    }
#if ipconfigUSE_IPv6 != 0
    {
        /* Why IPv4 failed, if IPv6 has no address to try either. */
        ebisu_sock_error_t error = sock->_error;
        long detail = sock->_error_detail;
        if (connect_family(sock, host, port, FREERTOS_AF_INET6)) {
            return EBISU_SOCK_OK;
        }
        if (sock->_error == EBISU_SOCK_ERROR_DNS && error != EBISU_SOCK_ERROR_DNS) {
            sock->_error = error;
            sock->_error_detail = detail;
        }
    }
#endif
    log_to(sock->_logger, EBISU_LOG_ERROR, "failed to %s %s:%u: %ld",
            sock->_error == EBISU_SOCK_ERROR_DNS ? "look up" : "connect to", host,
            (unsigned int)port, sock->_error_detail);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t tcp_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    BaseType_t ret = FreeRTOS_send(sock->_socket, buffer, length, 0);
    *out_sent_length = 0;
    if (ret > 0) {
        *out_sent_length = (size_t)ret;
        return EBISU_SOCK_OK;
    }
    if (ret == 0 || ret == -pdFREERTOS_ERRNO_ENOSPC) {
        /* Its timeout ran out with nothing sent. */
        log_to(sock->_logger, EBISU_LOG_ERROR, "failed to send: timed out");
        return fail_with(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
    }
    log_to(sock->_logger, EBISU_LOG_ERROR, "failed to send: %ld", (long)-ret);
    return fail_with(sock, ret == -pdFREERTOS_ERRNO_ENOTCONN ?
            EBISU_SOCK_ERROR_CLOSED : EBISU_SOCK_ERROR_OTHER, (long)-ret);
}

static ebisu_sock_code_t tcp_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    BaseType_t ret = FreeRTOS_recv(sock->_socket, buffer, length_to_read, 0);
    *out_actual_length = 0;
    sock->_recv_timed_out = false;
    if (ret > 0) {
        *out_actual_length = (size_t)ret;
        return EBISU_SOCK_OK;
    }
    if (ret == 0) {
        /* Its timeout ran out with nothing received: routine on an idle
           connection, which goes on as it was. */
        sock->_recv_timed_out = true;
        return fail_with(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
    }
    if (ret == -pdFREERTOS_ERRNO_ENOTCONN) {
        /* The peer closed or reset the connection: the end of the stream. */
        return EBISU_SOCK_OK;
    }
    log_to(sock->_logger, EBISU_LOG_ERROR, "failed to receive: %ld", (long)-ret);
    return fail_with(sock, EBISU_SOCK_ERROR_OTHER, (long)-ret);
}

/* Shut down, then wait for the peer to close its side, as FreeRTOS-Plus-TCP
   has a connection closed gracefully, before the socket goes. */
static ebisu_sock_code_t tcp_close(ebisu_socket_t* self)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    const TickType_t wait = ms_to_ticks(CLOSE_WAIT_MS);
    const TickType_t turn = ms_to_ticks(CLOSE_TURN_MS);
    TickType_t start;
    char discarded[64];
    if (sock->_socket == FREERTOS_INVALID_SOCKET) {
        return EBISU_SOCK_OK;
    }
    (void)FreeRTOS_shutdown(sock->_socket, FREERTOS_SHUT_RDWR);
    set_timeouts(sock->_socket, turn, turn);
    start = xTaskGetTickCount();
    while (FreeRTOS_recv(sock->_socket, discarded, sizeof(discarded), 0) >= 0 &&
            (TickType_t)(xTaskGetTickCount() - start) < wait) {
    }
    (void)FreeRTOS_closesocket(sock->_socket);
    sock->_socket = FREERTOS_INVALID_SOCKET;
    return EBISU_SOCK_OK;
}

static bool tcp_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    return sock->_recv_timed_out;
}

static ebisu_sock_error_t tcp_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    *out_detail = sock->_error_detail;
    return sock->_error;
}

/* A receive that peeks waits for data as any other does, for the timeout it is
   given here, and leaves the data for recv. One that fails, as for a peer
   that closed, is for recv to return at once too. */
static bool tcp_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_freertos_tcp_socket_t* sock =
        EBISU_CONTAINER_OF(self, ebisu_freertos_tcp_socket_t, base);
    TickType_t wait = timeout_ms == 0 ? 0 : ms_to_ticks(timeout_ms);
    TickType_t to_recv = timeout_ticks(sock->_to_recv);
    char peeked;
    BaseType_t ret;
    (void)FreeRTOS_setsockopt(sock->_socket, 0, FREERTOS_SO_RCVTIMEO, &wait, sizeof(wait));
    ret = FreeRTOS_recv(sock->_socket, &peeked, 1, FREERTOS_MSG_PEEK);
    (void)FreeRTOS_setsockopt(sock->_socket, 0, FREERTOS_SO_RCVTIMEO, &to_recv, sizeof(to_recv));
    return ret != 0;
}

static const ebisu_socket_ops_t tcp_ops = {
    tcp_connect,
    tcp_send,
    tcp_recv,
    tcp_close,
    tcp_recv_timed_out,
    tcp_last_error,
    tcp_recv_wait
};

void ebisu_freertos_tcp_socket_init(
        ebisu_freertos_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = &tcp_ops;
    sock->_socket = FREERTOS_INVALID_SOCKET;
    sock->_to_recv = to_recv;
    sock->_to_send = to_send;
    sock->_to_connect = to_send;
}

void ebisu_freertos_tcp_socket_set_connect_timeout(ebisu_freertos_tcp_socket_t* sock,
        unsigned int seconds)
{
    sock->_to_connect = seconds;
}

void ebisu_freertos_tcp_socket_set_logger(ebisu_freertos_tcp_socket_t* sock,
        ebisu_logger_t* logger)
{
    sock->_logger = logger;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
