/* Looking a host up and connecting to it, over BSD sockets, for the sockets of
   platform's POSIX and OpenSSL ports: the TCP that comes before the TLS of
   one, and all there is of the other. Each library compiles it in, static, so
   that neither needs the other, and each has the lookups of its own sockets.

   Not a header of the API: it defines what it declares, and is included once,
   by a source file that has defined log_to(), as ebisu_log() or nothing. It
   needs _DEFAULT_SOURCE under -std=c99, for what glibc declares of POSIX. */
#ifndef EBISU_POSIX_CONNECT_H
#define EBISU_POSIX_CONNECT_H

#include <ebisu_log.h>
#include <ebisu_socket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int64_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Waits for fd to be ready for events until deadline_ms, or for ever if it is
   0. 1 when it is, 0 when the deadline passed, -1 on failure. */
static int wait_for(int fd, short events, int64_t deadline_ms)
{
    while (1) {
        int timeout_ms = -1;
        if (deadline_ms > 0) {
            int64_t left = deadline_ms - monotonic_ms();
            if (left <= 0) {
                return 0;
            }
            timeout_ms = left > 60000 ? 60000 : (int)left;
        }
        struct pollfd p;
        p.fd = fd;
        p.events = events;
        p.revents = 0;
        int ret = poll(&p, 1, timeout_ms);
        if (ret > 0) {
            return 1;
        }
        if (ret < 0 && errno != EINTR) {
            return -1;
        }
    }
}

/* A lookup on a thread of its own, which a connecting task waits for no longer
   than its timeout. A task that gives up leaves it running, to free what they
   share by itself once done, and a task that looks the same host and port up
   meanwhile waits for it too, rather than start another: however often
   connects time out on a lookup that doesn't end, they leave one thread
   waiting for it. Its addresses are shared by the tasks that waited for them,
   and freed with the last. */
typedef struct lookup_t {
    /* Signalled once the lookup is done. */
    pthread_cond_t done_cond;
    /* The thread, until it is done, and each task waiting for it or using its
       addresses. Freed at none. */
    int users;
    char host[256];
    char service[8];
    int done;
    int error;
    struct addrinfo* result;
    /* The next of the lookups still running. */
    struct lookup_t* next;
} lookup_t;

/* Guards every lookup_t, and the list of those still running. */
static pthread_mutex_t lookups_mutex = PTHREAD_MUTEX_INITIALIZER;
static lookup_t* running_lookups = NULL;

/* Initializes cond to time its waits by the monotonic clock, as the deadlines
   are, so that setting the time of day doesn't move them. 0 on success. */
static int init_done_cond(pthread_cond_t* cond)
{
#ifdef __APPLE__
    /* macOS has no pthread_condattr_setclock(). wait_for_lookup() waits for a
       time from now instead. */
    return pthread_cond_init(cond, NULL);
#else
    pthread_condattr_t attr;
    if (pthread_condattr_init(&attr) != 0) {
        return -1;
    }
    int ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (ret == 0) {
        ret = pthread_cond_init(cond, &attr);
    }
    pthread_condattr_destroy(&attr);
    return ret;
#endif
}

/* Waits, with lookups_mutex held, until lookup is done or deadline_ms passes.
   1 when it is done, 0 when the deadline passed first. */
static int wait_for_lookup(lookup_t* lookup, int64_t deadline_ms)
{
    while (!lookup->done) {
        int64_t left = deadline_ms - monotonic_ms();
        if (left <= 0) {
            return 0;
        }
        struct timespec ts;
#ifdef __APPLE__
        ts.tv_sec = (time_t)(left / 1000);
        ts.tv_nsec = (long)(left % 1000) * 1000000;
        pthread_cond_timedwait_relative_np(&lookup->done_cond, &lookups_mutex, &ts);
#else
        ts.tv_sec = (time_t)(deadline_ms / 1000);
        ts.tv_nsec = (long)(deadline_ms % 1000) * 1000000;
        pthread_cond_timedwait(&lookup->done_cond, &lookups_mutex, &ts);
#endif
    }
    return 1;
}

/* Drops a user of lookup, with lookups_mutex held, and frees it after the
   last. */
static void drop_user(lookup_t* lookup)
{
    if (--lookup->users > 0) {
        return;
    }
    if (lookup->result != NULL) {
        freeaddrinfo(lookup->result);
    }
    pthread_cond_destroy(&lookup->done_cond);
    free(lookup);
}

static void* run_lookup(void* arg)
{
    lookup_t* lookup = (lookup_t*)arg;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* result = NULL;
    int error = getaddrinfo(lookup->host, lookup->service, &hints, &result);
    pthread_mutex_lock(&lookups_mutex);
    lookup->error = error;
    lookup->result = result;
    lookup->done = 1;
    /* Done, so that a task looking the host up from now on starts again. */
    for (lookup_t** at = &running_lookups; *at != NULL; at = &(*at)->next) {
        if (*at == lookup) {
            *at = lookup->next;
            break;
        }
    }
    pthread_cond_broadcast(&lookup->done_cond);
    drop_user(lookup);
    pthread_mutex_unlock(&lookups_mutex);
    return NULL;
}

/* The running lookup of host and service, with a user more, or a new one, with
   the thread and the task for users, or NULL if it can't be started. With
   lookups_mutex held. */
static lookup_t* join_lookup(const char* host, const char* service)
{
    lookup_t* lookup;
    for (lookup = running_lookups; lookup != NULL; lookup = lookup->next) {
        if (strcmp(lookup->host, host) == 0 && strcmp(lookup->service, service) == 0) {
            ++lookup->users;
            return lookup;
        }
    }
    lookup = (lookup_t*)calloc(1, sizeof(lookup_t));
    if (lookup == NULL) {
        return NULL;
    }
    if (init_done_cond(&lookup->done_cond) != 0) {
        free(lookup);
        return NULL;
    }
    lookup->users = 2;
    strcpy(lookup->host, host);
    strcpy(lookup->service, service);
    pthread_t thread;
    if (pthread_create(&thread, NULL, run_lookup, lookup) != 0) {
        pthread_cond_destroy(&lookup->done_cond);
        free(lookup);
        return NULL;
    }
    pthread_detach(thread);
    lookup->next = running_lookups;
    running_lookups = lookup;
    return lookup;
}

/* The host's addresses, NULL if they couldn't be looked up by deadline_ms, or
   ever if that is 0. They are freed with free_addresses(), and *out_lookup,
   which it takes too. */
static struct addrinfo* look_up(const char* host, uint16_t port, int64_t deadline_ms,
        ebisu_logger_t* logger, lookup_t** out_lookup, ebisu_sock_error_t* out_error,
        long* out_detail)
{
    *out_lookup = NULL;
    *out_error = EBISU_SOCK_ERROR_OTHER;
    *out_detail = 0;
    char service[8];
    snprintf(service, sizeof(service), "%u", (unsigned int)port);
    if (deadline_ms == 0) {
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo* result = NULL;
        int error = getaddrinfo(host, service, &hints, &result);
        if (error != 0) {
            log_to(logger, EBISU_LOG_ERROR, "failed to look up %s: %s", host,
                    gai_strerror(error));
            *out_error = EBISU_SOCK_ERROR_DNS;
            *out_detail = error;
            return NULL;
        }
        return result;
    }

    if (strlen(host) >= sizeof(((lookup_t*)0)->host)) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up a host name that long.");
        *out_error = EBISU_SOCK_ERROR_DNS;
        return NULL;
    }
    pthread_mutex_lock(&lookups_mutex);
    lookup_t* lookup = join_lookup(host, service);
    if (lookup == NULL) {
        pthread_mutex_unlock(&lookups_mutex);
        return NULL;
    }
    int done = wait_for_lookup(lookup, deadline_ms);
    int error = lookup->error;
    if (!done || error != 0) {
        drop_user(lookup);
        lookup = NULL;
    }
    pthread_mutex_unlock(&lookups_mutex);
    if (!done) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up %s in time.", host);
        *out_error = EBISU_SOCK_ERROR_TIMEOUT;
        return NULL;
    }
    if (error != 0) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up %s: %s", host, gai_strerror(error));
        *out_error = EBISU_SOCK_ERROR_DNS;
        *out_detail = error;
        return NULL;
    }
    /* Read, not changed, by the tasks that share them, until the last frees
       them. */
    *out_lookup = lookup;
    return lookup->result;
}

/* Frees addresses look_up() gave, with the lookup it gave them from. */
static void free_addresses(struct addrinfo* addrs, lookup_t* lookup)
{
    if (lookup == NULL) {
        freeaddrinfo(addrs);
        return;
    }
    pthread_mutex_lock(&lookups_mutex);
    drop_user(lookup);
    pthread_mutex_unlock(&lookups_mutex);
}

/* Connects sock to addr by deadline_ms, or whenever TCP gives up if that is
   0. 0 when connected, -1 otherwise, with why in out_error and out_errno. */
static int connect_by(int sock, const struct sockaddr* addr, socklen_t addr_len,
        int64_t deadline_ms, ebisu_sock_error_t* out_error, int* out_errno)
{
    *out_error = EBISU_SOCK_ERROR_CONNECT;
    *out_errno = 0;
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        *out_error = EBISU_SOCK_ERROR_OTHER;
        *out_errno = errno;
        return -1;
    }
    int ret = connect(sock, addr, addr_len);
    if (ret != 0) {
        *out_errno = errno;
    }
    if (ret != 0 && errno == EINPROGRESS) {
        ret = -1;
        int ready = wait_for(sock, POLLOUT, deadline_ms);
        if (ready == 1) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                ret = 0;
            } else {
                *out_errno = error;
            }
        } else if (ready == 0) {
            *out_error = EBISU_SOCK_ERROR_TIMEOUT;
            *out_errno = 0;
        }
    }
    /* Blocking again, for the handshake and what follows, which the receive
       and send timeouts bound. */
    if (fcntl(sock, F_SETFL, flags) != 0) {
        return -1;
    }
    return ret;
}

/* A socket connected to one of host's addresses, tried in the order the
   resolver gives them, or -1. */
static int connect_to_host(const char* host, uint16_t port, unsigned int timeout_s,
        ebisu_logger_t* logger, ebisu_sock_error_t* out_error, long* out_detail)
{
    int64_t timeout_ms = (int64_t)timeout_s * 1000;
    int64_t deadline_ms = timeout_ms > 0 ? monotonic_ms() + timeout_ms : 0;
    lookup_t* lookup = NULL;
    struct addrinfo* addrs = look_up(host, port, deadline_ms, logger, &lookup, out_error,
            out_detail);
    if (addrs == NULL) {
        return -1;
    }
    /* Why the last address failed, which is what is said. */
    *out_error = EBISU_SOCK_ERROR_CONNECT;
    *out_detail = 0;
    int sock = -1;
    for (struct addrinfo* ai = addrs; ai != NULL; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) {
            *out_error = EBISU_SOCK_ERROR_OTHER;
            *out_detail = errno;
            continue;
        }
        /* Each address gets the whole timeout: one that doesn't answer leaves
           the rest as much time as the first had. */
        int64_t attempt_deadline_ms = timeout_ms > 0 ? monotonic_ms() + timeout_ms : 0;
        int attempt_errno = 0;
        if (connect_by(sock, ai->ai_addr, ai->ai_addrlen, attempt_deadline_ms,
                    out_error, &attempt_errno) == 0) {
            break;
        }
        *out_detail = attempt_errno;
        close(sock);
        sock = -1;
    }
    free_addresses(addrs, lookup);
    if (sock < 0) {
        log_to(logger, EBISU_LOG_ERROR, "failed to connect to %s.", host);
    }
    return sock;
}

/* Keepalive and TCP_NODELAY, as a socket of these sets them for each
   connection: keepalive off with idle_s 0, as TCP starts, and otherwise on,
   with its interval and count where they aren't 0. */
typedef struct tcp_options_t {
    unsigned int idle_s;
    unsigned int interval_s;
    unsigned int count;
    bool nodelay;
} tcp_options_t;

/* Sets an int option, or says why it couldn't be: the connection goes on
   without it. */
static void set_int_option(int sock, int level, int name, const char* what,
        unsigned int value, ebisu_logger_t* logger)
{
    int v = value > INT_MAX ? INT_MAX : (int)value;
    if (setsockopt(sock, level, name, &v, sizeof(v)) != 0) {
        int saved_errno = errno;
        (void)saved_errno;
        log_to(logger, EBISU_LOG_WARNING, "failed to set %s to %u: errno=%d: %s", what,
                value, saved_errno, strerror(saved_errno));
    }
}

/* Says an option was asked for that the system has no way to set. */
#define unsettable(logger, what) \
    log_to((logger), EBISU_LOG_WARNING, "%s can't be set on this system.", (what))

/* Sets options on a socket just connected. */
static void set_tcp_options(int sock, const tcp_options_t* options, ebisu_logger_t* logger)
{
    if (options->idle_s > 0) {
        set_int_option(sock, SOL_SOCKET, SO_KEEPALIVE, "SO_KEEPALIVE", 1, logger);
#if defined(TCP_KEEPIDLE)
        set_int_option(sock, IPPROTO_TCP, TCP_KEEPIDLE, "TCP_KEEPIDLE", options->idle_s,
                logger);
#elif defined(TCP_KEEPALIVE)
        /* macOS's name for it. */
        set_int_option(sock, IPPROTO_TCP, TCP_KEEPALIVE, "TCP_KEEPALIVE", options->idle_s,
                logger);
#else
        unsettable(logger, "The keepalive idle time");
#endif
        if (options->interval_s > 0) {
#ifdef TCP_KEEPINTVL
            set_int_option(sock, IPPROTO_TCP, TCP_KEEPINTVL, "TCP_KEEPINTVL",
                    options->interval_s, logger);
#else
            unsettable(logger, "TCP_KEEPINTVL");
#endif
        }
        if (options->count > 0) {
#ifdef TCP_KEEPCNT
            set_int_option(sock, IPPROTO_TCP, TCP_KEEPCNT, "TCP_KEEPCNT", options->count,
                    logger);
#else
            unsettable(logger, "TCP_KEEPCNT");
#endif
        }
    }
    if (options->nodelay) {
        set_int_option(sock, IPPROTO_TCP, TCP_NODELAY, "TCP_NODELAY", 1, logger);
    }
}

#endif /* EBISU_POSIX_CONNECT_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
