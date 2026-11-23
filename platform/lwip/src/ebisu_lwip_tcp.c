#include "ebisu_lwip.h"

#include <ebisu_object.h>

#include <limits.h>
#include <string.h>

#include <lwip/opt.h>
#include <lwip/init.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>

#include "ebisu_lwip_impl.h"

#if NO_SYS
#error "ebisu_lwip_tcp_socket_t is over lwIP's sockets, which need an OS: set NO_SYS to 0."
#endif
#if !LWIP_SOCKET
#error "ebisu_lwip_tcp_socket_t is over lwIP's sockets: set LWIP_SOCKET to 1."
#endif
#if !LWIP_TCP
#error "ebisu_lwip_tcp_socket_t is TCP: set LWIP_TCP to 1."
#endif
#if !LWIP_DNS
#error "ebisu_lwip_tcp_socket_t looks hosts up: set LWIP_DNS to 1."
#endif
#if !LWIP_SO_RCVTIMEO || !LWIP_SO_SNDTIMEO
#error "ebisu_lwip_tcp_socket_t has timeouts: set LWIP_SO_RCVTIMEO and LWIP_SO_SNDTIMEO to 1."
#endif

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)
#endif

static ebisu_sock_code_t fail_with(ebisu_lwip_tcp_socket_t* sock, ebisu_sock_error_t error,
        long detail)
{
    sock->_error = error;
    sock->_error_detail = detail;
    return EBISU_SOCK_FAIL;
}

/* Sets a socket's receive or send timeout, in seconds, 0 for none, as lwIP
   takes it: an int of milliseconds in lwIP 1.4, and with
   LWIP_SO_SNDRCVTIMEO_NONSTANDARD, a struct timeval otherwise. */
static void set_timeout(int s, int option, unsigned int seconds)
{
#if EBISU_LWIP_TIMEOUT_MS
    int ms = seconds > (unsigned int)INT_MAX / 1000u ? INT_MAX : (int)(seconds * 1000u);
    (void)lwip_setsockopt(s, SOL_SOCKET, option, &ms, sizeof(ms));
#else
    struct timeval tv;
    tv.tv_sec = (long)seconds;
    tv.tv_usec = 0;
    (void)lwip_setsockopt(s, SOL_SOCKET, option, &tv, sizeof(tv));
#endif
}

static void set_non_blocking(int s, bool non_blocking)
{
    int flags = lwip_fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    (void)lwip_fcntl(s, F_SETFL, non_blocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}

static void warn_refused(ebisu_logger_t* logger, const char* name, int value, int err)
{
    (void)logger;
    (void)name;
    (void)value;
    (void)err;
    log_to(logger, EBISU_LOG_WARNING, "%s %d refused: errno=%d; connecting without it", name,
            value, err);
}

static void set_int_option(int s, int level, int option, const char* name, int value,
        ebisu_logger_t* logger)
{
    if (lwip_setsockopt(s, level, option, &value, sizeof(value)) != 0) {
        warn_refused(logger, name, value, ebisu_lwip_error(s));
    }
}

/* What a connection made is set to: its timeouts, keepalive and TCP_NODELAY. */
static void set_options(ebisu_lwip_tcp_socket_t* sock, int s)
{
    set_timeout(s, SO_RCVTIMEO, sock->_to_recv);
    set_timeout(s, SO_SNDTIMEO, sock->_to_send);
    if (sock->_keepalive_idle > 0) {
        set_int_option(s, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", 1, sock->_logger);
#if LWIP_TCP_KEEPALIVE
        set_int_option(s, IPPROTO_TCP, TCP_KEEPIDLE, "TCP_KEEPIDLE",
                (int)sock->_keepalive_idle, sock->_logger);
        if (sock->_keepalive_interval > 0) {
            set_int_option(s, IPPROTO_TCP, TCP_KEEPINTVL, "TCP_KEEPINTVL",
                    (int)sock->_keepalive_interval, sock->_logger);
        }
        if (sock->_keepalive_count > 0) {
            set_int_option(s, IPPROTO_TCP, TCP_KEEPCNT, "TCP_KEEPCNT",
                    (int)sock->_keepalive_count, sock->_logger);
        }
#else
        log_to(sock->_logger, EBISU_LOG_WARNING,
                "lwIP is built without LWIP_TCP_KEEPALIVE: keepalive probes after its "
                "default idle time, interval and count");
#endif
    }
    if (sock->_nodelay) {
        set_int_option(s, IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY", 1, sock->_logger);
    }
}

/* Connects to the address, within the connect timeout. The socket, or -1 with
   why not the socket's error. */
static int connect_to(ebisu_lwip_tcp_socket_t* sock, const struct addrinfo* address)
{
    int s = lwip_socket(address->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        fail_with(sock, EBISU_SOCK_ERROR_OTHER, ebisu_lwip_error(-1));
        return -1;
    }
    set_non_blocking(s, true);
    int err = 0;
    if (lwip_connect(s, address->ai_addr, address->ai_addrlen) != 0) {
        err = ebisu_lwip_error(s);
        if (err == EINPROGRESS) {
            int ready = ebisu_lwip_wait(s, true,
                    sock->_to_connect == 0 ? -1 : (long)sock->_to_connect * 1000);
            if (ready == 0) {
                err = ETIMEDOUT;
            } else if (ready < 0) {
                err = ebisu_lwip_error(s);
            } else {
                socklen_t len = sizeof(err);
                if (lwip_getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
                    err = ebisu_lwip_error(s);
                }
#if LWIP_VERSION_MAJOR < 2
                /* lwIP 1.4's netconn_connect() records ERR_INPROGRESS once the
                   connect it started has gone on in lwIP's thread: a
                   connection made before that, as over the loopback, is left
                   with it, which SO_ERROR gives. A failure is not, as nothing
                   overwrites one, and the socket is writable only once
                   connected or failed. */
                if (err == EINPROGRESS) {
                    err = 0;
                }
#endif
            }
        }
    }
    if (err != 0) {
        fail_with(sock, err == ETIMEDOUT ? EBISU_SOCK_ERROR_TIMEOUT : EBISU_SOCK_ERROR_CONNECT,
                err);
        (void)lwip_close(s);
        return -1;
    }
    set_non_blocking(s, false);
    set_options(sock, s);
    return s;
}

/* How long a lookup waits for a result of lwIP's MEMP_NETDB pool that another
   task's holds: 50 tries, 10 ms apart, half a second. */
#define LOOKUP_TRIES 50u
#define LOOKUP_RETRY_MS 10u

/* The port in decimal, as lwip_getaddrinfo() takes it. */
static void format_port(uint16_t port, char out[6])
{
    char digits[5];
    int n = 0;
    do {
        digits[n++] = (char)('0' + port % 10u);
        port = (uint16_t)(port / 10u);
    } while (port > 0);
    for (int i = 0; i < n; ++i) {
        out[i] = digits[n - 1 - i];
    }
    out[n] = '\0';
}

#if defined(EBISU_TLS_RESUME)
/* How long an address is connected to again after the lookup that gave it. */
#define ADDRESS_LIFETIME_MS ((uint64_t)3600 * 1000)

/* What the socket keeps of an address: a sockaddr_in6 at most. */
typedef union {
    struct sockaddr any;
    struct sockaddr_in in;
#if LWIP_IPV6
    struct sockaddr_in6 in6;
#endif
} kept_address_t;
typedef char kept_address_fits[sizeof(kept_address_t) <= 28 ? 1 : -1];

/* FNV-1a, of 32 bits, of a host's name: the address is kept for the host
   without its name. */
static uint32_t host_hash(const char* host)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char* p = (const unsigned char*)host; *p != '\0'; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

/* A connection to the address kept for host and port, if there is one,
   looked up less than ADDRESS_LIFETIME_MS ago: the socket, or -1, the address
   then forgotten. */
static int connect_to_kept(ebisu_lwip_tcp_socket_t* sock, const char* host, uint16_t port)
{
    if (sock->_memory_system == NULL || sock->_last_address_size == 0 ||
            sock->_last_host != host_hash(host) || sock->_last_port != port ||
            ebisu_system_monotonic_ms(sock->_memory_system) - sock->_last_looked_up_ms >=
                ADDRESS_LIFETIME_MS) {
        sock->_last_address_size = 0;
        return -1;
    }
    kept_address_t kept;
    memset(&kept, 0, sizeof(kept));
    memcpy(&kept, sock->_last_address, sock->_last_address_size);
    struct addrinfo address;
    memset(&address, 0, sizeof(address));
    address.ai_family = kept.any.sa_family;
    address.ai_socktype = SOCK_STREAM;
    address.ai_protocol = IPPROTO_TCP;
    address.ai_addrlen = sock->_last_address_size;
    address.ai_addr = &kept.any;
    int s = connect_to(sock, &address);
    if (s < 0) {
        /* Gone, or not answering: the host is looked up, and the error is
           that connection's. */
        log_to(sock->_logger, EBISU_LOG_DEBUG,
                "the address of the last connection to %s didn't connect: looking it up.",
                host);
        sock->_last_address_size = 0;
        sock->_error = EBISU_SOCK_ERROR_NONE;
        sock->_error_detail = 0;
    }
    return s;
}

/* Keeps the address of a connection made after a lookup. */
static void keep_address(ebisu_lwip_tcp_socket_t* sock, const char* host, uint16_t port,
        const struct addrinfo* address)
{
    if (sock->_memory_system == NULL || address->ai_addrlen > sizeof(sock->_last_address)) {
        return;
    }
    memcpy(sock->_last_address, address->ai_addr, address->ai_addrlen);
    sock->_last_address_size = (unsigned char)address->ai_addrlen;
    sock->_last_host = host_hash(host);
    sock->_last_port = port;
    sock->_last_looked_up_ms = ebisu_system_monotonic_ms(sock->_memory_system);
}
#endif

static ebisu_sock_code_t tcp_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    struct addrinfo hints;
    struct addrinfo* results = NULL;
    char service[6];
    sock->_error = EBISU_SOCK_ERROR_NONE;
    sock->_error_detail = 0;
    sock->_recv_timed_out = false;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
#if defined(EBISU_TLS_RESUME)
    int kept = connect_to_kept(sock, host, port);
    if (kept >= 0) {
        sock->_socket = kept;
        return EBISU_SOCK_OK;
    }
#endif
    format_port(port, service);
    /* An address as written is taken as it is, without asking a server. The
       result is one of lwIP's MEMP_NUM_NETDB, 1 unless lwipopts.h says more,
       which another task's lookup may hold for a moment, as this one does:
       EAI_MEMORY, while it does. */
    int ret;
    unsigned int tries = 0;
    while ((ret = lwip_getaddrinfo(host, service, &hints, &results)) == EAI_MEMORY &&
            ++tries < LOOKUP_TRIES) {
        sys_msleep(LOOKUP_RETRY_MS);
    }
    if (ret != 0 || results == NULL) {
        fail_with(sock, EBISU_SOCK_ERROR_DNS, ret);
        log_to(sock->_logger, EBISU_LOG_ERROR, "failed to look up %s: %d", host, ret);
        return EBISU_SOCK_FAIL;
    }
    /* The address, which lwIP gives one of, out of the result, which goes
       back to lwIP before the connect, which may take the connect timeout. */
    union {
        struct sockaddr any;
        struct sockaddr_in in;
#if LWIP_IPV6
        struct sockaddr_in6 in6;
#endif
    } addr;
    struct addrinfo address = *results;
    memset(&addr, 0, sizeof(addr));
    memcpy(&addr, results->ai_addr,
            results->ai_addrlen < sizeof(addr) ? results->ai_addrlen : sizeof(addr));
    address.ai_addr = &addr.any;
    address.ai_canonname = NULL;
    address.ai_next = NULL;
    lwip_freeaddrinfo(results);
    int s = connect_to(sock, &address);
    if (s < 0) {
        log_to(sock->_logger, EBISU_LOG_ERROR, "failed to connect to %s:%u: errno=%ld", host,
                (unsigned int)port, sock->_error_detail);
        return EBISU_SOCK_FAIL;
    }
#if defined(EBISU_TLS_RESUME)
    keep_address(sock, host, port, &address);
#endif
    sock->_socket = s;
    return EBISU_SOCK_OK;
}

/* What errno is, after a send or a receive on a connection made. */
static ebisu_sock_error_t io_error(int err)
{
    if (err == EAGAIN || err == EWOULDBLOCK) {
        /* The socket's timeout, as it blocks. */
        return EBISU_SOCK_ERROR_TIMEOUT;
    }
    if (err == EPIPE || err == ECONNRESET || err == ENOTCONN || err == ECONNABORTED) {
        return EBISU_SOCK_ERROR_CLOSED;
    }
    return EBISU_SOCK_ERROR_OTHER;
}

static ebisu_sock_code_t tcp_send(ebisu_socket_t* self, const char* buffer, size_t length,
        size_t* out_sent_length)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    *out_sent_length = 0;
    /* An int in lwIP 2.0, an ssize_t from 2.1. */
    long sent = (long)lwip_send(sock->_socket, buffer, length, 0);
    if (sent >= 0) {
        *out_sent_length = (size_t)sent;
        return EBISU_SOCK_OK;
    }
    int err = ebisu_lwip_error(sock->_socket);
    log_to(sock->_logger, EBISU_LOG_ERROR, "failed to send: errno=%d", err);
    return fail_with(sock, io_error(err), err);
}

static ebisu_sock_code_t tcp_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
        size_t* out_actual_length)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    *out_actual_length = 0;
    sock->_recv_timed_out = false;
    long received = (long)lwip_recv(sock->_socket, buffer, length_to_read, 0);
    if (received >= 0) {
        /* 0 is the end of the stream. */
        *out_actual_length = (size_t)received;
        return EBISU_SOCK_OK;
    }
    int err = ebisu_lwip_error(sock->_socket);
    if (err == EAGAIN || err == EWOULDBLOCK) {
        /* The receive timeout: routine on an idle connection, which goes on
           as it was. Not logged. */
        sock->_recv_timed_out = true;
        return fail_with(sock, EBISU_SOCK_ERROR_TIMEOUT, 0);
    }
    log_to(sock->_logger, EBISU_LOG_ERROR, "failed to receive: errno=%d", err);
    return fail_with(sock, io_error(err), err);
}

static ebisu_sock_code_t tcp_close(ebisu_socket_t* self)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    if (sock->_socket >= 0) {
        (void)lwip_close(sock->_socket);
        sock->_socket = -1;
    }
    return EBISU_SOCK_OK;
}

static bool tcp_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    return sock->_recv_timed_out;
}

static ebisu_sock_error_t tcp_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    *out_detail = sock->_error_detail;
    return sock->_error;
}

/* Data, the end of the stream and a failure all make the socket readable, and
   recv then returns without waiting. A failure to wait is for recv to
   report. */
static bool tcp_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_lwip_tcp_socket_t* sock = EBISU_CONTAINER_OF(self, ebisu_lwip_tcp_socket_t, base);
    return ebisu_lwip_wait(sock->_socket, false, (long)(timeout_ms > 0x7fffffffu ? 0x7fffffffu :
                timeout_ms)) != 0;
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

void ebisu_lwip_tcp_socket_init(
        ebisu_lwip_tcp_socket_t* sock,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(sock, 0, sizeof(*sock));
    sock->base.ops = &tcp_ops;
    sock->_socket = -1;
    sock->_to_recv = to_recv;
    sock->_to_send = to_send;
    sock->_to_connect = to_send;
}

void ebisu_lwip_tcp_socket_set_connect_timeout(ebisu_lwip_tcp_socket_t* sock,
        unsigned int seconds)
{
    sock->_to_connect = seconds;
}

void ebisu_lwip_tcp_socket_set_logger(ebisu_lwip_tcp_socket_t* sock,
        ebisu_logger_t* logger)
{
    sock->_logger = logger;
}

#if defined(EBISU_TLS_RESUME)
void ebisu_lwip_tcp_socket_set_address_memory(ebisu_lwip_tcp_socket_t* sock,
        ebisu_system_t* system)
{
    sock->_memory_system = system;
    if (system == NULL) {
        sock->_last_address_size = 0;
    }
}
#endif

void ebisu_lwip_tcp_socket_set_keepalive(ebisu_lwip_tcp_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count)
{
    sock->_keepalive_idle = idle_s;
    sock->_keepalive_interval = interval_s;
    sock->_keepalive_count = count;
}

void ebisu_lwip_tcp_socket_set_nodelay(ebisu_lwip_tcp_socket_t* sock, bool nodelay)
{
    sock->_nodelay = nodelay;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
