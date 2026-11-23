#include "ebisu_win32.h"

#include <winsock2.h>

#include <limits.h>
#include <string.h>

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

/* Looking the host up and connecting, with log_to() defined above. */
#include "ebisu_win32_connect.h"

static ebisu_sock_code_t tcp_connect(ebisu_socket_t* self, const char* host, uint16_t port);
static ebisu_sock_code_t tcp_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length);
static ebisu_sock_code_t tcp_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length);
static ebisu_sock_code_t tcp_close(ebisu_socket_t* self);
static bool tcp_recv_timed_out(ebisu_socket_t* self);
static ebisu_sock_error_t tcp_last_error(ebisu_socket_t* self, long* out_detail);
static bool tcp_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms);

static const ebisu_socket_ops_t tcp_ops = {
    tcp_connect,
    tcp_send,
    tcp_recv,
    tcp_close,
    tcp_recv_timed_out,
    tcp_last_error,
    tcp_recv_wait
};

void ebisu_win32_tcp_socket_init(
        ebisu_win32_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = &tcp_ops;
    sock->socket = (uintptr_t)INVALID_SOCKET;
    sock->to_recv = to_recv;
    sock->to_send = to_send;
    sock->to_connect = to_send;
}

void ebisu_win32_tcp_socket_set_connect_timeout(ebisu_win32_tcp_socket_t* sock,
        unsigned int seconds)
{
    sock->to_connect = seconds;
}

void ebisu_win32_tcp_socket_set_logger(ebisu_win32_tcp_socket_t* sock,
        ebisu_logger_t* logger)
{
    sock->logger = logger;
}

void ebisu_win32_tcp_socket_set_keepalive(ebisu_win32_tcp_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count)
{
    sock->keepalive_idle = idle_s;
    sock->keepalive_interval = interval_s;
    sock->keepalive_count = count;
}

void ebisu_win32_tcp_socket_set_nodelay(ebisu_win32_tcp_socket_t* sock, bool nodelay)
{
    sock->nodelay = nodelay;
}

/* Says why an operation failed, for last_error, and returns EBISU_SOCK_FAIL. */
static ebisu_sock_code_t fail_with(ebisu_win32_tcp_socket_t* ctx, ebisu_sock_error_t error,
        long detail)
{
    ctx->error = error;
    ctx->error_detail = detail;
    return EBISU_SOCK_FAIL;
}

/* What a Winsock error is, on a connection made. */
static ebisu_sock_error_t wsa_error(int code)
{
    switch (code) {
        case WSAETIMEDOUT:
            return EBISU_SOCK_ERROR_TIMEOUT;
        case WSAECONNRESET:
        case WSAECONNABORTED:
        case WSAESHUTDOWN:
        case WSAENOTCONN:
            return EBISU_SOCK_ERROR_CLOSED;
        default:
            return EBISU_SOCK_ERROR_OTHER;
    }
}

/* Waits for sock to have something for recv() to return, data, the end of the
   stream or a failure, for timeout_ms, or for ever if that is INFINITE. 1 when
   it has, 0 when the time ran out first, SOCKET_ERROR on failure. */
static int wait_readable(SOCKET sock, DWORD timeout_ms)
{
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(sock, &readable);
    struct timeval timeout;
    timeout.tv_sec = (long)(timeout_ms / 1000);
    timeout.tv_usec = (long)(timeout_ms % 1000) * 1000;
    /* Winsock ignores the first argument. */
    return select(0, &readable, NULL, NULL, timeout_ms == INFINITE ? NULL : &timeout);
}

static ebisu_sock_code_t tcp_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    ctx->recv_timed_out = false;
    ebisu_sock_error_t error = EBISU_SOCK_ERROR_OTHER;
    long detail = 0;
    SOCKET sock = connect_to_host(host, port, ctx->to_connect, ctx->logger, &error, &detail,
            ctx->last_address, &ctx->last_address_size);
    if (sock == INVALID_SOCKET) {
        return fail_with(ctx, error, detail);
    }
    /* A send that cannot complete within the timeout fails, and the
       connection is given up. There is no SO_RCVTIMEO: tcp_recv() waits for
       the receive timeout itself. */
    if (ctx->to_send > 0) {
        DWORD ms = ctx->to_send > MAXDWORD / 1000 ? MAXDWORD : ctx->to_send * 1000;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&ms, sizeof(ms)) != 0) {
            int code = WSAGetLastError();
            log_code_to(ctx->logger, (unsigned long)code, "setsockopt(SO_SNDTIMEO)");
            closesocket(sock);
            return fail_with(ctx, EBISU_SOCK_ERROR_OTHER, code);
        }
    }
    const tcp_options_t options = {
        ctx->keepalive_idle, ctx->keepalive_interval, ctx->keepalive_count, ctx->nodelay
    };
    set_tcp_options(sock, &options, ctx->logger);
    ctx->socket = (uintptr_t)sock;
    return EBISU_SOCK_OK;
}

static ebisu_sock_code_t tcp_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    *out_sent_length = 0;
    int chunk = length > INT_MAX ? INT_MAX : (int)length;
    int sent = send((SOCKET)ctx->socket, buffer, chunk, 0);
    if (sent == SOCKET_ERROR) {
        int code = WSAGetLastError();
        log_code_to(ctx->logger, (unsigned long)code, "failed to send");
        return fail_with(ctx, wsa_error(code), code);
    }
    *out_sent_length = (size_t)sent;
    return EBISU_SOCK_OK;
}

/* The wait is select()'s, rather than a recv() ended by SO_RCVTIMEO: Winsock
   documents a socket on which a receive has timed out as being in an
   indeterminate state, and liable to lose data, and the MQTT task goes on
   using its socket after every idle timeout. A recv() that only starts once
   select() has seen data, the end of the stream or an error never blocks, so
   a timeout here leaves the socket as it was. */
static ebisu_sock_code_t tcp_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    SOCKET sock = (SOCKET)ctx->socket;
    *out_actual_length = 0;
    ctx->recv_timed_out = false;
    DWORD timeout_ms = ctx->to_recv == 0 ? INFINITE :
        ctx->to_recv >= (INFINITE - 1) / 1000 ? INFINITE - 1 : ctx->to_recv * 1000;
    int ready = wait_readable(sock, timeout_ms);
    if (ready == 0) {
        /* The receive timeout. Not EBISU_SOCK_AGAIN, on which khc reads again
           at once, and for good if the server has gone. Not logged: on an
           idle connection it is routine. */
        ctx->recv_timed_out = true;
        return fail_with(ctx, EBISU_SOCK_ERROR_TIMEOUT, 0);
    }
    if (ready == SOCKET_ERROR) {
        int code = WSAGetLastError();
        log_code_to(ctx->logger, (unsigned long)code, "failed to wait to receive");
        return fail_with(ctx, EBISU_SOCK_ERROR_OTHER, code);
    }
    int chunk = length_to_read > INT_MAX ? INT_MAX : (int)length_to_read;
    int received = recv(sock, buffer, chunk, 0);
    if (received == SOCKET_ERROR) {
        int code = WSAGetLastError();
        log_code_to(ctx->logger, (unsigned long)code, "failed to receive");
        return fail_with(ctx, wsa_error(code), code);
    }
    /* 0 is the end of the stream. */
    *out_actual_length = (size_t)received;
    return EBISU_SOCK_OK;
}

static bool tcp_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    return ctx->recv_timed_out;
}

static ebisu_sock_error_t tcp_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    *out_detail = ctx->error_detail;
    return ctx->error;
}

/* A failure of select() is for tcp_recv() to report. */
static bool tcp_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    DWORD ms = timeout_ms >= INFINITE ? INFINITE - 1 : (DWORD)timeout_ms;
    return wait_readable((SOCKET)ctx->socket, ms) != 0;
}

static ebisu_sock_code_t tcp_close(ebisu_socket_t* self)
{
    ebisu_win32_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_win32_tcp_socket_t, base);
    if ((SOCKET)ctx->socket != INVALID_SOCKET) {
        closesocket((SOCKET)ctx->socket);
        ctx->socket = (uintptr_t)INVALID_SOCKET;
    }
    return EBISU_SOCK_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
