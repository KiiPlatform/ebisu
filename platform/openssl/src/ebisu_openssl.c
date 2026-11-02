#include "ebisu_openssl.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <errno.h>

/* TLS 1.2's suites with forward secrecy and authenticated encryption, Mozilla's
   intermediate list. */
#define TLS12_CIPHERS \
    "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:" \
    "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:" \
    "ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:" \
    "DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-CHACHA20-POLY1305"

/* Suppress warnings, because OpenSSL was deprecated in Mac. */
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
/* Before 1.1.0, OpenSSL leaves its locking to the application, and without it
   the tasks' connections corrupt its state. The ID of a thread it takes from
   the address of errno, which is one per thread here. */
static pthread_mutex_t openssl_locks[CRYPTO_NUM_LOCKS];

static void openssl_lock(int mode, int n, const char* file, int line)
{
    (void)file;
    (void)line;
    if (mode & CRYPTO_LOCK) {
        pthread_mutex_lock(&openssl_locks[n]);
    } else {
        pthread_mutex_unlock(&openssl_locks[n]);
    }
}
#endif

static void init_openssl(void)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    for (int i = 0; i < CRYPTO_NUM_LOCKS; ++i) {
        pthread_mutex_init(&openssl_locks[i], NULL);
    }
    CRYPTO_set_locking_callback(openssl_lock);
#endif
    SSL_library_init();
    SSL_load_error_strings();
}

/* The tasks connect at the same time, and it is not safe to run twice. */
static void ensure_openssl(void)
{
    static pthread_once_t openssl_initialized = PTHREAD_ONCE_INIT;
    pthread_once(&openssl_initialized, init_openssl);
}

static int verify_callback(int preverify_ok, X509_STORE_CTX* store);

#if defined(__GNUC__) || defined(__clang__)
static void log_to(ebisu_logger_t* logger, ebisu_log_level_t level, const char* fmt, ...)
    __attribute__((format(printf, 3, 4)));
#endif

/* Sends a diagnostic to logger, or to stderr without one. */
static void log_to(ebisu_logger_t* logger, ebisu_log_level_t level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (logger != NULL) {
        logger->ops->log(logger, level, fmt, args);
    } else {
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
    }
    va_end(args);
}

/* An SSL_CTX trusting the CA certificates given, or the system's without any. */
static SSL_CTX* new_ssl_ctx(const char* ca_file, const char* ca_path)
{
    ensure_openssl();
    const SSL_METHOD *method =
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        TLSv1_2_client_method();
#else
        TLS_client_method();
#endif
    SSL_CTX* ssl_ctx = SSL_CTX_new(method);
    if (ssl_ctx == NULL){
        return NULL;
    }

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    /* Servers that answer "Connection: close" drop the transport without
       sending a TLS close_notify. OpenSSL 1.1.1 surfaced that as
       SSL_ERROR_SYSCALL with errno 0, i.e. an ordinary EOF; 3.0 turned it into
       a hard "unexpected eof while reading" error instead. This option asks
       for the old, lenient behaviour. */
    SSL_CTX_set_options(ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
    /* TLS 1.2 at least, as TLSv1_2_client_method() is for 1.0.2. Left to
       itself, OpenSSL 1.1 goes down to TLS 1.0. */
    if (SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION) != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }
#endif
    /* TLS 1.2's suites with forward secrecy and authenticated encryption,
       Mozilla's intermediate list, rather than whatever OpenSSL's build
       defaults to. Those an OpenSSL lacks, ChaCha20 before 1.1.0, are left
       out. TLS 1.3's are all of that kind, and stay as OpenSSL has them. */
    if (SSL_CTX_set_cipher_list(ssl_ctx, TLS12_CIPHERS) != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    /* The server's certificate has to chain to one of these. */
    int loaded = (ca_file != NULL || ca_path != NULL) ?
        SSL_CTX_load_verify_locations(ssl_ctx, ca_file, ca_path) :
        SSL_CTX_set_default_verify_paths(ssl_ctx);
    if (loaded != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);
    return ssl_ctx;
}

/* The one sockets without a configuration of their own share, made at the
   first connection. NULL if that failed. */
static SSL_CTX* default_ssl_ctx = NULL;

static void init_default_ssl_ctx(void)
{
    default_ssl_ctx = new_ssl_ctx(NULL, NULL);
}

static SSL_CTX* shared_ssl_ctx(ebisu_openssl_socket_t* sock)
{
    if (sock->tls != NULL) {
        return sock->tls->ssl_ctx;
    }
    static pthread_once_t made = PTHREAD_ONCE_INIT;
    pthread_once(&made, init_default_ssl_ctx);
    return default_ssl_ctx;
}

bool ebisu_openssl_tls_init(ebisu_openssl_tls_t* tls, const char* ca_file, const char* ca_path)
{
    tls->ssl_ctx = new_ssl_ctx(ca_file, ca_path);
    return tls->ssl_ctx != NULL;
}

void ebisu_openssl_tls_free(ebisu_openssl_tls_t* tls)
{
    SSL_CTX_free(tls->ssl_ctx);
    tls->ssl_ctx = NULL;
}

static ebisu_sock_code_t openssl_connect(ebisu_socket_t* self, const char* host,
        uint16_t port);
static ebisu_sock_code_t openssl_send(ebisu_socket_t* self, const char* buffer,
        size_t length, size_t* out_sent_length);
static ebisu_sock_code_t openssl_recv(ebisu_socket_t* self, char* buffer,
        size_t length_to_read, size_t* out_actual_length);
static ebisu_sock_code_t openssl_close(ebisu_socket_t* self);
static bool openssl_recv_timed_out(ebisu_socket_t* self);

static const ebisu_socket_ops_t openssl_ops = {
    openssl_connect,
    openssl_send,
    openssl_recv,
    openssl_close,
    openssl_recv_timed_out
};

void ebisu_openssl_socket_init(
        ebisu_openssl_socket_t* ctx,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.ops = &openssl_ops;
    ctx->socket = -1;
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
    ctx->to_connect = to_send;
}

void ebisu_openssl_socket_set_connect_timeout(ebisu_openssl_socket_t* sock, unsigned int seconds)
{
    sock->to_connect = seconds;
}

void ebisu_openssl_socket_set_tls(ebisu_openssl_socket_t* sock, ebisu_openssl_tls_t* tls)
{
    sock->tls = tls;
}

void ebisu_openssl_socket_set_clock(ebisu_openssl_socket_t* sock, ebisu_clock_t* clock)
{
    sock->clock = clock;
}

void ebisu_openssl_socket_set_logger(ebisu_openssl_socket_t* sock, ebisu_logger_t* logger)
{
    sock->logger = logger;
}

/* OpenSSL has checked the server's certificate chain, its host name and the
   validity period of each certificate, at the clock's time when there is a
   clock. While the clock has only a lower bound of the time, its floor, a
   certificate that is not valid yet cannot be told from one that is: it may
   simply have been issued after the floor. That alone is accepted here.
   Everything else OpenSSL refused stands, a certificate that expired before
   the floor included.

   A certificate not valid yet at a time the clock had as exact is refused,
   and the time forgotten: more likely than a certificate from the future, the
   time fell behind, the monotonic clock having stopped while the device slept
   and the server having renewed its certificate meanwhile. Checked against a
   lower bound, the next connection goes through, and its response gives the
   clock the time again. */
static int verify_callback(int preverify_ok, X509_STORE_CTX* store)
{
    if (preverify_ok) {
        return 1;
    }
    SSL* ssl = X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
    ebisu_openssl_socket_t* ctx = (ebisu_openssl_socket_t*)SSL_get_app_data(ssl);
    if (X509_STORE_CTX_get_error(store) == X509_V_ERR_CERT_NOT_YET_VALID &&
            ctx->clock != NULL) {
        if (!ctx->time_exact) {
            return 1;
        }
        ebisu_clock_forget_time(ctx->clock);
    }
    return 0;
}

/* What SSL_get_error() answers depends on this thread's error queue being empty
   before the call it is asked about, and errno says something only if that call
   set it. A failure earlier on the thread leaves both behind: the task making a
   request is often the one whose last connection failed. */
static void clear_errors(void)
{
    ERR_clear_error();
    errno = 0;
}

static void log_ssl_failure(ebisu_logger_t* logger, const char* what, int ssl_error,
        int saved_errno)
{
    if (ssl_error == SSL_ERROR_SYSCALL) {
        log_to(logger, EBISU_LOG_ERROR, "failed to %s: errno=%d: %s", what, saved_errno,
                strerror(saved_errno));
    } else if (ssl_error == SSL_ERROR_SSL) {
        char sslErrStr[120];
        ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
        log_to(logger, EBISU_LOG_ERROR, "failed to %s: %s", what, sslErrStr);
    } else {
        log_to(logger, EBISU_LOG_ERROR, "failed to %s: SSL_get_error()=%d", what, ssl_error);
    }
}

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

/* A lookup on a thread of its own, which the connecting task waits for no
   longer than its timeout. A task that gives up leaves it to finish and free
   what they share by itself. */
typedef struct {
    pthread_mutex_t mutex;
    /* Signalled once the lookup is done. */
    pthread_cond_t done_cond;
    /* Two while both use it, then one, then none, when it is freed. */
    int users;
    char host[256];
    char service[8];
    /* Set with the mutex held. */
    int done;
    int error;
    struct addrinfo* result;
} lookup_t;

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

/* Waits, with lookup's mutex held, until it is done or deadline_ms passes. 1
   when it is done, 0 when the deadline passed first. */
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
        pthread_cond_timedwait_relative_np(&lookup->done_cond, &lookup->mutex, &ts);
#else
        ts.tv_sec = (time_t)(deadline_ms / 1000);
        ts.tv_nsec = (long)(deadline_ms % 1000) * 1000000;
        pthread_cond_timedwait(&lookup->done_cond, &lookup->mutex, &ts);
#endif
    }
    return 1;
}

static void release_lookup(lookup_t* lookup)
{
    pthread_mutex_lock(&lookup->mutex);
    int users = --lookup->users;
    pthread_mutex_unlock(&lookup->mutex);
    if (users > 0) {
        return;
    }
    if (lookup->result != NULL) {
        freeaddrinfo(lookup->result);
    }
    pthread_cond_destroy(&lookup->done_cond);
    pthread_mutex_destroy(&lookup->mutex);
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
    pthread_mutex_lock(&lookup->mutex);
    lookup->error = error;
    lookup->result = result;
    lookup->done = 1;
    pthread_cond_signal(&lookup->done_cond);
    pthread_mutex_unlock(&lookup->mutex);
    release_lookup(lookup);
    return NULL;
}

/* The host's addresses, NULL if it couldn't be looked up by deadline_ms, or
   ever if that is 0. */
static struct addrinfo* look_up(const char* host, uint16_t port, int64_t deadline_ms,
        ebisu_logger_t* logger)
{
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
            return NULL;
        }
        return result;
    }

    if (strlen(host) >= sizeof(((lookup_t*)0)->host)) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up a host name that long.");
        return NULL;
    }
    lookup_t* lookup = (lookup_t*)calloc(1, sizeof(lookup_t));
    if (lookup == NULL) {
        return NULL;
    }
    if (init_done_cond(&lookup->done_cond) != 0) {
        free(lookup);
        return NULL;
    }
    pthread_mutex_init(&lookup->mutex, NULL);
    lookup->users = 2;
    strcpy(lookup->host, host);
    strcpy(lookup->service, service);
    pthread_t thread;
    if (pthread_create(&thread, NULL, run_lookup, lookup) != 0) {
        lookup->users = 1;
        release_lookup(lookup);
        return NULL;
    }
    pthread_detach(thread);

    struct addrinfo* result = NULL;
    pthread_mutex_lock(&lookup->mutex);
    int done = wait_for_lookup(lookup, deadline_ms);
    int error = lookup->error;
    if (done && error == 0) {
        result = lookup->result;
        lookup->result = NULL;
    }
    pthread_mutex_unlock(&lookup->mutex);
    if (!done) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up %s in time.", host);
    } else if (error != 0) {
        log_to(logger, EBISU_LOG_ERROR, "failed to look up %s: %s", host, gai_strerror(error));
    }
    release_lookup(lookup);
    return result;
}

/* Connects sock to addr by deadline_ms, or whenever TCP gives up if that is
   0. 0 when connected, -1 otherwise. */
static int connect_by(int sock, const struct sockaddr* addr, socklen_t addr_len,
        int64_t deadline_ms)
{
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        return -1;
    }
    int ret = connect(sock, addr, addr_len);
    if (ret != 0 && errno == EINPROGRESS) {
        ret = -1;
        if (wait_for(sock, POLLOUT, deadline_ms) == 1) {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0) {
                ret = 0;
            }
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
        ebisu_logger_t* logger)
{
    int64_t timeout_ms = (int64_t)timeout_s * 1000;
    int64_t deadline_ms = timeout_ms > 0 ? monotonic_ms() + timeout_ms : 0;
    struct addrinfo* addrs = look_up(host, port, deadline_ms, logger);
    if (addrs == NULL) {
        return -1;
    }
    int sock = -1;
    for (struct addrinfo* ai = addrs; ai != NULL; ai = ai->ai_next) {
        sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sock < 0) {
            continue;
        }
        /* Each address gets the whole timeout: one that doesn't answer leaves
           the rest as much time as the first had. */
        int64_t attempt_deadline_ms = timeout_ms > 0 ? monotonic_ms() + timeout_ms : 0;
        if (connect_by(sock, ai->ai_addr, ai->ai_addrlen, attempt_deadline_ms) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(addrs);
    if (sock < 0) {
        log_to(logger, EBISU_LOG_ERROR, "failed to connect to %s.", host);
    }
    return sock;
}

static ebisu_sock_code_t
    openssl_connect(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    int sock, ret;
    SSL *ssl = NULL;
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);

    sock = connect_to_host(host, port, ctx->to_connect, ctx->logger);
    if (sock < 0) {
        return EBISU_SOCK_FAIL;
    }

    if (ctx->to_recv > 0) {
        struct timeval tv;
        tv.tv_sec = ctx->to_recv;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    }

    if (ctx->to_send > 0) {
        struct timeval tv;
        tv.tv_sec = ctx->to_send;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    }

    /* Shared with the other connections: the CA certificates are loaded once. */
    SSL_CTX* ssl_ctx = shared_ssl_ctx(ctx);
    if (ssl_ctx == NULL) {
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to set up TLS.");
        goto fail;
    }
    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL){
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to init ssl.");
        goto fail;
    }
    SSL_set_app_data(ssl, ctx);

    /* The name the server is to present a certificate for, and the one the
       certificate has to be issued for. */
    SSL_set_tlsext_host_name(ssl, host);
    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (X509_VERIFY_PARAM_set1_host(param, host, 0) != 1) {
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to set the host name to verify.");
        goto fail;
    }

    /* Checked at the clock's time: its floor until a server has stated the
       time, that time from then on. Without a clock, at the system's. */
    if (ctx->clock != NULL) {
        int64_t now = 0;
        ctx->time_exact = ebisu_clock_get_time(ctx->clock, &now);
        /* A 32-bit time_t ends in 2038. Cut down to fit one, the time would
           be decades out, and certificates refused for what they are not. */
        if ((int64_t)(time_t)now != now) {
            log_to(ctx->logger, EBISU_LOG_ERROR, "the time does not fit in a time_t.");
            goto fail;
        }
        X509_VERIFY_PARAM_set_time(param, (time_t)now);
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret == 0){
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to set fd.");
        goto fail;
    }

    clear_errors();
    ret = SSL_connect(ssl);
    if (ret != 1) {
        long verified = SSL_get_verify_result(ssl);
        if (verified != X509_V_OK) {
            log_to(ctx->logger, EBISU_LOG_ERROR, "server certificate for %s rejected: %s",
                    host, X509_verify_cert_error_string(verified));
        } else {
            char sslErrStr[120];
            ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
            log_to(ctx->logger, EBISU_LOG_ERROR, "failed to connect: %s", sslErrStr);
        }
        goto fail;
    }

    ctx->socket = sock;
    ctx->ssl = ssl;
    ctx->ssl_failed = false;
    return EBISU_SOCK_OK;

fail:
    if (ssl != NULL) {
        SSL_free(ssl);
    }
    close(sock);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t
    openssl_send(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    clear_errors();
    int ret = SSL_write(ctx->ssl, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return EBISU_SOCK_OK;
    }
    /* errno first, before anything else can change it. */
    int saved_errno = errno;
    int ssl_error = SSL_get_error(ctx->ssl, ret);
    if (ssl_error == SSL_ERROR_SYSCALL || ssl_error == SSL_ERROR_SSL) {
        ctx->ssl_failed = true;
    }
    log_ssl_failure(ctx->logger, "send", ssl_error, saved_errno);
    return EBISU_SOCK_FAIL;
}

static ebisu_sock_code_t
    openssl_recv(ebisu_socket_t* self,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    *out_actual_length = 0;
    clear_errors();
    int ret = SSL_read(ctx->ssl, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return EBISU_SOCK_OK;
    }
    /* errno first, before anything else can change it. */
    ctx->recv_errno = errno;
    ctx->recv_ssl_error = SSL_get_error(ctx->ssl, ret);
    switch (ctx->recv_ssl_error) {
    case SSL_ERROR_ZERO_RETURN:
        return EBISU_SOCK_OK;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* The receive timeout, as these sockets block. Not EBISU_SOCK_AGAIN,
           on which khc reads again at once, and for good if the server has
           gone. See openssl_recv_timed_out(). */
        return EBISU_SOCK_FAIL;
    case SSL_ERROR_SYSCALL:
        ctx->ssl_failed = true;
        /* The end of the stream, from a peer that sent no close_notify. */
        if (ctx->recv_errno == 0) {
            return EBISU_SOCK_OK;
        }
        break;
    case SSL_ERROR_SSL:
        ctx->ssl_failed = true;
        break;
    default:
        break;
    }
    log_ssl_failure(ctx->logger, "receive", ctx->recv_ssl_error, ctx->recv_errno);
    return EBISU_SOCK_FAIL;
}

/* openssl_recv() returns EBISU_SOCK_FAIL both when the receive timeout set with
   SO_RCVTIMEO in openssl_connect() runs out and when the connection fails.
   The timeout makes the read fail with EAGAIN or EWOULDBLOCK, which OpenSSL
   reports as SSL_ERROR_WANT_READ, and errno is only meaningful with
   SSL_ERROR_SYSCALL. */
static bool
    openssl_recv_timed_out(ebisu_socket_t* self)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    if (ctx->recv_ssl_error == SSL_ERROR_WANT_READ) {
        return true;
    }
    if (ctx->recv_ssl_error == SSL_ERROR_SYSCALL &&
            (ctx->recv_errno == EAGAIN || ctx->recv_errno == EWOULDBLOCK)) {
        return true;
    }
    return false;
}

static ebisu_sock_code_t
    openssl_close(ebisu_socket_t* self)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    /* 1 means the bidirectional shutdown completed. 0 means our close_notify
       went out but the peer's has not arrived yet, which is not an error and is
       the normal outcome against a server that closes the connection itself.
       Everything below is torn down regardless, so a unidirectional shutdown is
       all we need. SSL_get_error() is only meaningful for a negative return.
       See https://www.openssl.org/docs/man3.0/man3/SSL_shutdown.html */
    int shutdown_ok = 1;
    if (!ctx->ssl_failed) {
        clear_errors();
        int ret = SSL_shutdown(ctx->ssl);
        if (ret < 0) {
            int sslErr = SSL_get_error(ctx->ssl, ret);
            if (sslErr != SSL_ERROR_SYSCALL && sslErr != SSL_ERROR_ZERO_RETURN) {
                char sslErrStr[120];
                ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
                log_to(ctx->logger, EBISU_LOG_ERROR, "failed to shutdown: %s", sslErrStr);
                shutdown_ok = 0;
            }
            /* Otherwise the peer closed the transport without a close_notify,
               which is harmless while we are tearing the connection down. */
        }
    }
    close(ctx->socket);
    SSL_free(ctx->ssl);
    if (!shutdown_ok) {
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to close.");
        return EBISU_SOCK_FAIL;
    }
    return EBISU_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

