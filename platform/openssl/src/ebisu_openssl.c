#include "ebisu_openssl.h"

#include <sys/socket.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
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

#ifdef EBISU_NO_LOG
/* Built without logging, a diagnostic is left out, its message too, and its
   arguments aren't evaluated. */
#define log_to(logger, level, ...) ((void)0)
#define log_ssl_error(logger, what) ((void)0)
#define log_ssl_failure(logger, what, ssl_error, saved_errno) ((void)0)
#else
/* Sends a diagnostic to logger. Without one, it goes nowhere. */
#define log_to(logger, level, ...) ebisu_log((logger), (level), __VA_ARGS__)

/* Logs what failed with the first of OpenSSL's errors, which it takes off the
   queue. */
static void log_ssl_error(ebisu_logger_t* logger, const char* what)
{
    char sslErrStr[120];
    ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
    log_to(logger, EBISU_LOG_ERROR, "failed to %s: %s", what, sslErrStr);
}
#endif

/* The sessions of the last connections to a few hosts, each offered to the
   next connection to the same host and port to resume: the server may take it
   and skip most of the handshake, which on a slow link costs more than many a
   request. One per SSL_CTX, kept in its ex_data, so that the sockets sharing a
   configuration share them, and SSL_CTX_free() frees them. */
#define SESSION_CACHE_SIZE 4
#define SESSION_HOST_SIZE 256

typedef struct {
    char host[SESSION_HOST_SIZE];
    uint16_t port;
    SSL_SESSION* session; /* NULL for an entry not in use. */
} cached_session_t;

typedef struct {
    pthread_mutex_t lock;
    cached_session_t entries[SESSION_CACHE_SIZE];
    /* The entry the next host replaces when none is free. */
    unsigned int next;
} session_cache_t;

static int session_cache_index = -1;

static void free_session_cache(void* parent, void* ptr, CRYPTO_EX_DATA* ad,
        int idx, long argl, void* argp)
{
    (void)parent;
    (void)ad;
    (void)idx;
    (void)argl;
    (void)argp;
    session_cache_t* cache = (session_cache_t*)ptr;
    if (cache == NULL) {
        return;
    }
    for (int i = 0; i < SESSION_CACHE_SIZE; ++i) {
        if (cache->entries[i].session != NULL) {
            SSL_SESSION_free(cache->entries[i].session);
        }
    }
    pthread_mutex_destroy(&cache->lock);
    free(cache);
}

static void init_session_cache_index(void)
{
    session_cache_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, free_session_cache);
}

static session_cache_t* session_cache_of(SSL_CTX* ssl_ctx)
{
    if (session_cache_index < 0) {
        return NULL;
    }
    return (session_cache_t*)SSL_CTX_get_ex_data(ssl_ctx, session_cache_index);
}

/* The entry for host and port, or NULL. Called with the lock held. */
static cached_session_t* find_session(session_cache_t* cache, const char* host, uint16_t port)
{
    for (int i = 0; i < SESSION_CACHE_SIZE; ++i) {
        cached_session_t* entry = &cache->entries[i];
        if (entry->session != NULL && entry->port == port &&
                strcmp(entry->host, host) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* Offers ssl the session kept for host and port, if there is one it can
   resume. */
static void offer_session(SSL* ssl, const char* host, uint16_t port)
{
    session_cache_t* cache = session_cache_of(SSL_get_SSL_CTX(ssl));
    if (cache == NULL) {
        return;
    }
    pthread_mutex_lock(&cache->lock);
    cached_session_t* entry = find_session(cache, host, port);
#if (OPENSSL_VERSION_NUMBER >= 0x10101000L)
    if (entry != NULL && !SSL_SESSION_is_resumable(entry->session)) {
        entry = NULL;
    }
#endif
    if (entry != NULL) {
        /* Which takes a reference of its own. */
        SSL_set_session(ssl, entry->session);
    }
    pthread_mutex_unlock(&cache->lock);
}

/* Drops the session kept for host and port, after a handshake that failed,
   which it may have been the cause of. */
static void forget_session(SSL_CTX* ssl_ctx, const char* host, uint16_t port)
{
    session_cache_t* cache = session_cache_of(ssl_ctx);
    if (cache == NULL) {
        return;
    }
    pthread_mutex_lock(&cache->lock);
    cached_session_t* entry = find_session(cache, host, port);
    if (entry != NULL) {
        SSL_SESSION_free(entry->session);
        entry->session = NULL;
    }
    pthread_mutex_unlock(&cache->lock);
}

/* Called with each session the server gives: at the end of the handshake, or,
   in TLS 1.3, when its tickets are read, after it. Keeps the session, in place
   of the last one for the same host and port. Returning 1 keeps the reference
   it is given. */
static int on_new_session(SSL* ssl, SSL_SESSION* session)
{
    ebisu_openssl_socket_t* ctx = (ebisu_openssl_socket_t*)SSL_get_app_data(ssl);
    /* The name the server was asked for, which the certificate was checked
       against. */
    const char* host = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
    session_cache_t* cache = session_cache_of(SSL_get_SSL_CTX(ssl));
    if (ctx == NULL || host == NULL || cache == NULL ||
            strlen(host) >= SESSION_HOST_SIZE) {
        return 0;
    }
    pthread_mutex_lock(&cache->lock);
    cached_session_t* entry = find_session(cache, host, ctx->port);
    if (entry == NULL) {
        for (int i = 0; i < SESSION_CACHE_SIZE && entry == NULL; ++i) {
            if (cache->entries[i].session == NULL) {
                entry = &cache->entries[i];
            }
        }
    }
    if (entry == NULL) {
        entry = &cache->entries[cache->next];
        cache->next = (cache->next + 1) % SESSION_CACHE_SIZE;
    }
    if (entry->session != NULL) {
        SSL_SESSION_free(entry->session);
    }
    strcpy(entry->host, host);
    entry->port = ctx->port;
    entry->session = session;
    pthread_mutex_unlock(&cache->lock);
    return 1;
}

/* Gives ssl_ctx a cache of sessions to resume. Without one, as when memory
   runs out, connections make a full handshake each, and work all the same. */
static void add_session_cache(SSL_CTX* ssl_ctx)
{
    static pthread_once_t index_made = PTHREAD_ONCE_INIT;
    pthread_once(&index_made, init_session_cache_index);
    if (session_cache_index < 0) {
        return;
    }
    session_cache_t* cache = (session_cache_t*)calloc(1, sizeof(*cache));
    if (cache == NULL) {
        return;
    }
    if (pthread_mutex_init(&cache->lock, NULL) != 0) {
        free(cache);
        return;
    }
    if (SSL_CTX_set_ex_data(ssl_ctx, session_cache_index, cache) != 1) {
        pthread_mutex_destroy(&cache->lock);
        free(cache);
        return;
    }
    /* OpenSSL's own cache is the server's: a client's sessions are offered by
       the application, from the callback's. */
    SSL_CTX_set_session_cache_mode(ssl_ctx,
            SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    SSL_CTX_sess_set_new_cb(ssl_ctx, on_new_session);
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
    add_session_cache(ssl_ctx);
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
static ebisu_sock_error_t openssl_last_error(ebisu_socket_t* self, long* out_detail);
static bool openssl_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms);

static const ebisu_socket_ops_t openssl_ops = {
    openssl_connect,
    openssl_send,
    openssl_recv,
    openssl_close,
    openssl_recv_timed_out,
    openssl_last_error,
    openssl_recv_wait
};

/* Says why an operation failed, for last_error, and returns EBISU_SOCK_FAIL. */
static ebisu_sock_code_t fail_with(ebisu_openssl_socket_t* ctx,
        ebisu_sock_error_t error, long detail)
{
    ctx->error = error;
    ctx->error_detail = detail;
    return EBISU_SOCK_FAIL;
}

/* Why SSL_read() or SSL_write() failed, from SSL_get_error() and errno. */
static ebisu_sock_error_t io_error(int ssl_error, int saved_errno, long* out_detail)
{
    *out_detail = 0;
    switch (ssl_error) {
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* The socket's timeout, as these sockets block. */
        return EBISU_SOCK_ERROR_TIMEOUT;
    case SSL_ERROR_ZERO_RETURN:
        return EBISU_SOCK_ERROR_CLOSED;
    case SSL_ERROR_SYSCALL:
        *out_detail = saved_errno;
        if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
            return EBISU_SOCK_ERROR_TIMEOUT;
        }
        if (saved_errno == 0 || saved_errno == EPIPE || saved_errno == ECONNRESET) {
            return EBISU_SOCK_ERROR_CLOSED;
        }
        return EBISU_SOCK_ERROR_OTHER;
    case SSL_ERROR_SSL:
        *out_detail = (long)ERR_peek_error();
        return EBISU_SOCK_ERROR_TLS;
    default:
        *out_detail = ssl_error;
        return EBISU_SOCK_ERROR_OTHER;
    }
}

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

#ifndef EBISU_NO_LOG
static void log_ssl_failure(ebisu_logger_t* logger, const char* what, int ssl_error,
        int saved_errno)
{
    if (ssl_error == SSL_ERROR_SYSCALL) {
        log_to(logger, EBISU_LOG_ERROR, "failed to %s: errno=%d: %s", what, saved_errno,
                strerror(saved_errno));
    } else if (ssl_error == SSL_ERROR_SSL) {
        log_ssl_error(logger, what);
    } else {
        log_to(logger, EBISU_LOG_ERROR, "failed to %s: SSL_get_error()=%d", what, ssl_error);
    }
}
#endif

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

static ebisu_sock_code_t
    openssl_connect(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    int sock, ret;
    SSL *ssl = NULL;
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);

    ebisu_sock_error_t error = EBISU_SOCK_ERROR_OTHER;
    long detail = 0;
    sock = connect_to_host(host, port, ctx->to_connect, ctx->logger, &error, &detail);
    if (sock < 0) {
        return fail_with(ctx, error, detail);
    }
    /* What a failure below is, unless it says otherwise. */
    error = EBISU_SOCK_ERROR_TLS;
    detail = 0;

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
        error = EBISU_SOCK_ERROR_OTHER;
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
            error = EBISU_SOCK_ERROR_OTHER;
            goto fail;
        }
        X509_VERIFY_PARAM_set_time(param, (time_t)now);
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret == 0){
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to set fd.");
        error = EBISU_SOCK_ERROR_OTHER;
        goto fail;
    }

    /* For the sessions the server gives to be kept under. */
    ctx->port = port;
    offer_session(ssl, host, port);

    clear_errors();
    ret = SSL_connect(ssl);
    if (ret != 1) {
        /* errno first, before anything else can change it. */
        int saved_errno = errno;
        int ssl_error = SSL_get_error(ssl, ret);
        forget_session(ssl_ctx, host, port);
        long verified = SSL_get_verify_result(ssl);
        if (verified != X509_V_OK) {
            log_to(ctx->logger, EBISU_LOG_ERROR, "server certificate for %s rejected: %s",
                    host, X509_verify_cert_error_string(verified));
            error = EBISU_SOCK_ERROR_VERIFY;
            detail = verified;
        } else {
            /* A timeout, a peer that went, or TLS itself. */
            error = io_error(ssl_error, saved_errno, &detail);
            if (error == EBISU_SOCK_ERROR_OTHER || error == EBISU_SOCK_ERROR_TLS) {
                error = EBISU_SOCK_ERROR_TLS;
                detail = (long)ERR_peek_error();
            }
            log_ssl_error(ctx->logger, "connect");
        }
        goto fail;
    }

    if (SSL_session_reused(ssl)) {
        log_to(ctx->logger, EBISU_LOG_DEBUG, "resumed the TLS session with %s.", host);
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
    return fail_with(ctx, error, detail);
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
    long detail = 0;
    ebisu_sock_error_t error = io_error(ssl_error, saved_errno, &detail);
    log_ssl_failure(ctx->logger, "send", ssl_error, saved_errno);
    return fail_with(ctx, error, detail);
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
        return fail_with(ctx, EBISU_SOCK_ERROR_TIMEOUT, 0);
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
    long detail = 0;
    ebisu_sock_error_t error = io_error(ctx->recv_ssl_error, ctx->recv_errno, &detail);
    log_ssl_failure(ctx->logger, "receive", ctx->recv_ssl_error, ctx->recv_errno);
    return fail_with(ctx, error, detail);
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

/* What OpenSSL has read and decrypted, and not yet returned, is no longer on
   the socket, where poll() looks: SSL_pending() counts it. A failure of poll(),
   but for a signal, is for openssl_recv() to report. */
static bool
    openssl_recv_wait(ebisu_socket_t* self, uint32_t timeout_ms)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    if (SSL_pending(ctx->ssl) > 0) {
        return true;
    }
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

static ebisu_sock_error_t
    openssl_last_error(ebisu_socket_t* self, long* out_detail)
{
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);
    *out_detail = ctx->error_detail;
    return ctx->error;
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
                log_ssl_error(ctx->logger, "shutdown");
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

