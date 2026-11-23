#include "ebisu_posix.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

/* Looking the host up and connecting, with log_to() defined above. */
#include "ebisu_posix_connect.h"

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

void ebisu_posix_tcp_socket_init(
        ebisu_posix_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = &tcp_ops;
    sock->socket = -1;
    sock->to_recv = to_recv;
    sock->to_send = to_send;
    sock->to_connect = to_send;
}

void ebisu_posix_tcp_socket_set_connect_timeout(ebisu_posix_tcp_socket_t* sock,
        unsigned int seconds)
{
    sock->to_connect = seconds;
}

void ebisu_posix_tcp_socket_set_logger(ebisu_posix_tcp_socket_t* sock,
        ebisu_logger_t* logger)
{
    sock->logger = logger;
}

void ebisu_posix_tcp_socket_set_keepalive(ebisu_posix_tcp_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count)
{
    sock->keepalive_idle = idle_s;
    sock->keepalive_interval = interval_s;
    sock->keepalive_count = count;
}

void ebisu_posix_tcp_socket_set_nodelay(ebisu_posix_tcp_socket_t* sock, bool nodelay)
{
    sock->nodelay = nodelay;
}

/* Says why an operation failed, for last_error, and returns EBISU_SOCK_FAIL. */
static ebisu_sock_code_t fail_with(ebisu_posix_tcp_socket_t* ctx, ebisu_sock_error_t error,
        long detail)
{
    ctx->error = error;
    ctx->error_detail = detail;
    return EBISU_SOCK_FAIL;
}

/* What errno is, after a send() or recv() on a connection made. */
static ebisu_sock_error_t io_error(int saved_errno)
{
    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
        /* The socket's timeout, as these sockets block. */
        return EBISU_SOCK_ERROR_TIMEOUT;
    }
    if (saved_errno == EPIPE || saved_errno == ECONNRESET) {
        return EBISU_SOCK_ERROR_CLOSED;
    }
    return EBISU_SOCK_ERROR_OTHER;
}

static void set_timeout(int sock, int option, unsigned int seconds)
{
    if (seconds > 0) {
        struct timeval tv;
        tv.tv_sec = seconds;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, option, (const char*)&tv, sizeof(tv));
    }
}

static ebisu_sock_code_t tcp_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    ebisu_sock_error_t error = EBISU_SOCK_ERROR_OTHER;
    long detail = 0;
    int sock = connect_to_host(host, port, ctx->to_connect, ctx->logger, &error, &detail,
            ctx->last_address, &ctx->last_address_size);
    if (sock < 0) {
        return fail_with(ctx, error, detail);
    }
    set_timeout(sock, SO_RCVTIMEO, ctx->to_recv);
    set_timeout(sock, SO_SNDTIMEO, ctx->to_send);
    const tcp_options_t options = {
        ctx->keepalive_idle, ctx->keepalive_interval, ctx->keepalive_count, ctx->nodelay
    };
    set_tcp_options(sock, &options, ctx->logger);
#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
    /* Where send() can't be asked not to raise SIGPIPE, as on macOS, the
       socket is. */
    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif
    ctx->socket = sock;
    ctx->recv_errno = 0;
    return EBISU_SOCK_OK;
}

static ebisu_sock_code_t tcp_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    *out_sent_length = 0;
    int flags = 0;
#ifdef MSG_NOSIGNAL
    /* A peer that has gone makes send() fail with EPIPE, rather than end the
       program. */
    flags = MSG_NOSIGNAL;
#endif
    ssize_t sent;
    do {
        sent = send(ctx->socket, buffer, length, flags);
    } while (sent < 0 && errno == EINTR);
    if (sent >= 0) {
        *out_sent_length = (size_t)sent;
        return EBISU_SOCK_OK;
    }
    int saved_errno = errno;
    log_to(ctx->logger, EBISU_LOG_ERROR, "failed to send: errno=%d: %s", saved_errno,
            strerror(saved_errno));
    return fail_with(ctx, io_error(saved_errno), saved_errno);
}

static ebisu_sock_code_t tcp_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    *out_actual_length = 0;
    ssize_t received;
    do {
        received = recv(ctx->socket, buffer, length_to_read, 0);
    } while (received < 0 && errno == EINTR);
    if (received >= 0) {
        /* 0 is the end of the stream. */
        ctx->recv_errno = 0;
        *out_actual_length = (size_t)received;
        return EBISU_SOCK_OK;
    }
    ctx->recv_errno = errno;
    if (tcp_recv_timed_out(self)) {
        /* The receive timeout. Not EBISU_SOCK_AGAIN, on which khc reads again
           at once, and for good if the server has gone. Not logged: on an
           idle connection it is routine. */
        return fail_with(ctx, EBISU_SOCK_ERROR_TIMEOUT, 0);
    }
    log_to(ctx->logger, EBISU_LOG_ERROR, "failed to receive: errno=%d: %s", ctx->recv_errno,
            strerror(ctx->recv_errno));
    return fail_with(ctx, io_error(ctx->recv_errno), ctx->recv_errno);
}

/* tcp_recv() returns EBISU_SOCK_FAIL both when the receive timeout set with
   SO_RCVTIMEO in tcp_connect() runs out, which makes recv() fail with EAGAIN or
   EWOULDBLOCK, and when the connection fails. */
static bool tcp_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    return ctx->recv_errno == EAGAIN || ctx->recv_errno == EWOULDBLOCK;
}

static ebisu_sock_error_t tcp_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    *out_detail = ctx->error_detail;
    return ctx->error;
}

/* Data, the end of the stream and a failure all make the socket readable, and
   recv() then returns without waiting. A failure of poll(), but for a signal,
   is for tcp_recv() to report. */
static bool tcp_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    struct pollfd p;
    p.fd = ctx->socket;
    p.events = POLLIN;
    p.revents = 0;
    int ret = poll(&p, 1, timeout_ms > INT_MAX ? INT_MAX : (int)timeout_ms);
    if (ret < 0 && errno == EINTR) {
        return false;
    }
    return ret != 0;
}

static ebisu_sock_code_t tcp_close(ebisu_socket_t* self)
{
    ebisu_posix_tcp_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_posix_tcp_socket_t, base);
    if (ctx->socket >= 0) {
        close(ctx->socket);
        ctx->socket = -1;
    }
    return EBISU_SOCK_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
