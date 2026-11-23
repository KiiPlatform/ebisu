#include "ebisu_openssl.h"
#include <ebisu_dns.h>

#include <sys/socket.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
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

/* Looking the host up and connecting, with log_to() defined above. By its path
   from here, so that a build of this file by hand needs no include directory
   more. */
#include "../../posix/src/ebisu_posix_connect.h"

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

bool ebisu_openssl_tls_init(ebisu_openssl_tls_t* tls, const char* ca_file, const char* ca_path,
        unsigned long* out_error)
{
    /* For the first error left to be this call's. */
    ERR_clear_error();
    tls->ssl_ctx = new_ssl_ctx(ca_file, ca_path);
    if (out_error != NULL) {
        *out_error = tls->ssl_ctx != NULL ? 0 : ERR_peek_error();
    }
    return tls->ssl_ctx != NULL;
}

void ebisu_openssl_tls_free(ebisu_openssl_tls_t* tls)
{
    SSL_CTX_free(tls->ssl_ctx);
    tls->ssl_ctx = NULL;
}

/* How a configuration checks servers' certificates, other than in full. Kept
   in its SSL_CTX's ex_data, which the connections still open hold on to, and
   which SSL_CTX_free() frees. */
typedef struct {
    ebisu_tls_verify_t verify;
    unsigned char pin[EBISU_TLS_PIN_SIZE];
} verify_settings_t;

static int verify_settings_index = -1;

static void free_verify_settings(void* parent, void* ptr, CRYPTO_EX_DATA* ad,
        int idx, long argl, void* argp)
{
    (void)parent;
    (void)ad;
    (void)idx;
    (void)argl;
    (void)argp;
    free(ptr);
}

static void init_verify_settings_index(void)
{
    verify_settings_index = SSL_CTX_get_ex_new_index(0, NULL, NULL, NULL, free_verify_settings);
}

/* How ssl_ctx checks certificates. */
static ebisu_tls_verify_t verify_of(SSL_CTX* ssl_ctx, const unsigned char** out_pin)
{
    const verify_settings_t* settings = verify_settings_index < 0 ? NULL :
        (const verify_settings_t*)SSL_CTX_get_ex_data(ssl_ctx, verify_settings_index);
    if (settings == NULL) {
        return EBISU_TLS_VERIFY_FULL;
    }
    if (out_pin != NULL) {
        *out_pin = settings->pin;
    }
    return settings->verify;
}

/* Whether the SHA-256 of cert, or of its public key's SubjectPublicKeyInfo in
   DER, is pin. */
static bool matches_pin(X509* cert, const unsigned char* pin)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    if (cert == NULL) {
        return false;
    }
    if (X509_digest(cert, EVP_sha256(), md, &md_len) == 1 &&
            md_len == EBISU_TLS_PIN_SIZE && CRYPTO_memcmp(md, pin, EBISU_TLS_PIN_SIZE) == 0) {
        return true;
    }
    bool matches = false;
    EVP_PKEY* key = X509_get_pubkey(cert);
    unsigned char* der = NULL;
    int der_len = key == NULL ? -1 : i2d_PUBKEY(key, &der);
    if (der_len > 0 && EVP_Digest(der, (size_t)der_len, md, &md_len, EVP_sha256(), NULL) == 1 &&
            md_len == EBISU_TLS_PIN_SIZE) {
        matches = CRYPTO_memcmp(md, pin, EBISU_TLS_PIN_SIZE) == 0;
    }
    OPENSSL_free(der);
    EVP_PKEY_free(key);
    return matches;
}

/* In place of OpenSSL's check of the chain, for a configuration that pins the
   server's certificate or checks none. */
static int check_without_chain(X509_STORE_CTX* store, void* arg)
{
    (void)arg;
    SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
    const unsigned char* pin = NULL;
    switch (verify_of(SSL_get_SSL_CTX(ssl), &pin)) {
        case EBISU_TLS_VERIFY_NONE:
            break;
        case EBISU_TLS_VERIFY_PIN: {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
            X509* cert = store->cert;
#else
            X509* cert = X509_STORE_CTX_get0_cert(store);
#endif
            if (!matches_pin(cert, pin)) {
                X509_STORE_CTX_set_error(store, X509_V_ERR_CERT_REJECTED);
                return 0;
            }
            break;
        }
        default:
            return X509_verify_cert(store);
    }
    X509_STORE_CTX_set_error(store, X509_V_OK);
    return 1;
}

/* Drops every session kept: they were checked as the configuration was. */
static void forget_sessions(SSL_CTX* ssl_ctx)
{
    session_cache_t* cache = session_cache_of(ssl_ctx);
    if (cache == NULL) {
        return;
    }
    pthread_mutex_lock(&cache->lock);
    for (int i = 0; i < SESSION_CACHE_SIZE; ++i) {
        if (cache->entries[i].session != NULL) {
            SSL_SESSION_free(cache->entries[i].session);
            cache->entries[i].session = NULL;
        }
    }
    pthread_mutex_unlock(&cache->lock);
}

bool ebisu_openssl_tls_set_verify(ebisu_openssl_tls_t* tls, ebisu_tls_verify_t verify,
        const unsigned char* pin)
{
    static pthread_once_t index_made = PTHREAD_ONCE_INIT;
    if (tls->ssl_ctx == NULL) {
        return false;
    }
    switch (verify) {
        case EBISU_TLS_VERIFY_FULL:
        case EBISU_TLS_VERIFY_CHAIN:
        case EBISU_TLS_VERIFY_NONE:
            break;
        case EBISU_TLS_VERIFY_PIN:
            if (pin == NULL) {
                return false;
            }
            break;
        default:
            return false;
    }
    pthread_once(&index_made, init_verify_settings_index);
    if (verify_settings_index < 0) {
        return false;
    }
    verify_settings_t* settings =
        (verify_settings_t*)SSL_CTX_get_ex_data(tls->ssl_ctx, verify_settings_index);
    if (settings == NULL) {
        settings = (verify_settings_t*)calloc(1, sizeof(*settings));
        if (settings == NULL) {
            return false;
        }
        if (SSL_CTX_set_ex_data(tls->ssl_ctx, verify_settings_index, settings) != 1) {
            free(settings);
            return false;
        }
    }
    settings->verify = verify;
    if (verify == EBISU_TLS_VERIFY_PIN) {
        memcpy(settings->pin, pin, EBISU_TLS_PIN_SIZE);
    } else {
        memset(settings->pin, 0, EBISU_TLS_PIN_SIZE);
    }
    /* The chain is OpenSSL's to check, unless none is checked. */
    SSL_CTX_set_cert_verify_callback(tls->ssl_ctx,
            verify == EBISU_TLS_VERIFY_PIN || verify == EBISU_TLS_VERIFY_NONE ?
                check_without_chain : NULL, NULL);
    forget_sessions(tls->ssl_ctx);
    return true;
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

/* The header of the first record the server sent in a handshake, as OpenSSL
   reads it, before it checks its version. */
typedef struct first_record_t {
    bool seen;
    unsigned char header[5];
} first_record_t;

/* OpenSSL's message callback, for the first record's header. */
static void note_first_record(int write_p, int version, int content_type, const void* buf,
        size_t len, SSL* ssl, void* arg)
{
    first_record_t* first = (first_record_t*)arg;
    (void)version;
    (void)ssl;
    if (write_p == 0 && content_type == SSL3_RT_HEADER && len >= sizeof(first->header) &&
            !first->seen) {
        memcpy(first->header, buf, sizeof(first->header));
        first->seen = true;
    }
}

/* Whether a handshake failed as the server and the port have no TLS version or
   cipher suite in common, from the first of OpenSSL's errors: an alert of the
   server's that says so, or a version of its OpenSSL refused.
   SSL_R_WRONG_VERSION_NUMBER is also what OpenSSL says of a server that isn't
   TLS at all, so it counts only when the server's first record is one of an
   older TLS, as 1.0.2 says of a server that takes TLS 1.0 alone. */
static bool is_mismatch(unsigned long e, const first_record_t* first)
{
    if (ERR_GET_LIB(e) != ERR_LIB_SSL) {
        return false;
    }
    switch (ERR_GET_REASON(e)) {
    case SSL_R_SSLV3_ALERT_HANDSHAKE_FAILURE:
    case SSL_R_TLSV1_ALERT_PROTOCOL_VERSION:
    case SSL_R_TLSV1_ALERT_INSUFFICIENT_SECURITY:
    case SSL_R_UNSUPPORTED_PROTOCOL:
        return true;
    case SSL_R_WRONG_VERSION_NUMBER:
        /* A content type of TLS's, 20 to 23, and SSL 3.0 to TLS 1.1. */
        return first->seen && first->header[0] >= 20 && first->header[0] <= 23 &&
            first->header[1] == 3 && first->header[2] < 3;
    default:
        return false;
    }
}

/* OpenSSL writes to the socket with write(), which raises SIGPIPE when the
   peer has gone: by default the end of the process, where the plain TCP
   socket's send() only fails. Where a socket can be told not to raise it,
   SO_NOSIGPIPE, it is, at connect. Elsewhere the signal is blocked on the
   thread for as long as OpenSSL may write, and one the write raised, which is
   then pending on this thread alone, is taken off before the mask is put
   back: other threads' signals, the application's handler and its mask are
   as they were. A SIGPIPE pending before isn't this call's, and is left. */
typedef struct sigpipe_guard_t {
#ifndef SO_NOSIGPIPE
    sigset_t old_mask;
    bool blocked;
    bool was_pending;
#else
    char unused;
#endif
} sigpipe_guard_t;

static void sigpipe_block(sigpipe_guard_t* guard)
{
#ifndef SO_NOSIGPIPE
    sigset_t sigpipe;
    sigset_t pending;
    sigemptyset(&sigpipe);
    sigaddset(&sigpipe, SIGPIPE);
    guard->was_pending = sigpending(&pending) == 0 && sigismember(&pending, SIGPIPE) == 1;
    guard->blocked = pthread_sigmask(SIG_BLOCK, &sigpipe, &guard->old_mask) == 0;
#else
    (void)guard;
#endif
}

/* Leaves errno as it was, for the caller to read after the OpenSSL call. */
static void sigpipe_restore(sigpipe_guard_t* guard)
{
#ifndef SO_NOSIGPIPE
    int saved_errno = errno;
    if (!guard->blocked) {
        return;
    }
    if (!guard->was_pending) {
        const struct timespec no_wait = { 0, 0 };
        sigset_t sigpipe;
        sigemptyset(&sigpipe);
        sigaddset(&sigpipe, SIGPIPE);
        while (sigtimedwait(&sigpipe, NULL, &no_wait) == -1 && errno == EINTR) {
        }
    }
    pthread_sigmask(SIG_SETMASK, &guard->old_mask, NULL);
    errno = saved_errno;
#else
    (void)guard;
#endif
}

/* The handshake, by a deadline: to_connect seconds from now, where the
   receive and send timeouts alone bound each wait for the server, and not how
   many there are, so that a server that says nothing with no receive timeout,
   or a little at a time, would keep connect from returning. The socket
   doesn't block meanwhile, and blocks again after. What SSL_connect()
   returned, last: on a wait that ran out, less than 1, with
   SSL_ERROR_WANT_READ or SSL_ERROR_WANT_WRITE for SSL_get_error(), as a
   blocking socket's timeout leaves it. With to_connect of 0, as it was: one
   call, on the blocking socket. */
static int handshake(SSL* ssl, int sock, unsigned int to_recv, unsigned int to_send,
        unsigned int to_connect)
{
    if (to_connect == 0) {
        return SSL_connect(ssl);
    }
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        return SSL_connect(ssl);
    }
    const int64_t deadline_ms = monotonic_ms() + (int64_t)to_connect * 1000;
    int ret;
    for (;;) {
        ret = SSL_connect(ssl);
        if (ret == 1) {
            break;
        }
        int ssl_error = SSL_get_error(ssl, ret);
        if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
            break;
        }
        /* For the server until the deadline, or the receive or send timeout
           if that comes first. */
        const bool reading = (ssl_error == SSL_ERROR_WANT_READ);
        const unsigned int to_wait = reading ? to_recv : to_send;
        int64_t wait_until_ms = deadline_ms;
        if (to_wait > 0 && monotonic_ms() + (int64_t)to_wait * 1000 < deadline_ms) {
            wait_until_ms = monotonic_ms() + (int64_t)to_wait * 1000;
        }
        if (wait_for(sock, reading ? POLLIN : POLLOUT, wait_until_ms) != 1) {
            errno = EAGAIN;
            break;
        }
    }
    int saved_errno = errno;
    (void)fcntl(sock, F_SETFL, flags);
    errno = saved_errno;
    return ret;
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

void ebisu_openssl_socket_set_keepalive(ebisu_openssl_socket_t* sock,
        unsigned int idle_s, unsigned int interval_s, unsigned int count)
{
    sock->keepalive_idle = idle_s;
    sock->keepalive_interval = interval_s;
    sock->keepalive_count = count;
}

void ebisu_openssl_socket_set_nodelay(ebisu_openssl_socket_t* sock, bool nodelay)
{
    sock->nodelay = nodelay;
}

/* ctx->ssl is set once connected, and cleared as the connection is closed. */
const char* ebisu_openssl_socket_get_protocol(const ebisu_openssl_socket_t* sock)
{
    return sock->ssl != NULL ? SSL_get_version(sock->ssl) : NULL;
}

const char* ebisu_openssl_socket_get_cipher(const ebisu_openssl_socket_t* sock)
{
    return sock->ssl != NULL ? SSL_get_cipher_name(sock->ssl) : NULL;
}

/* OpenSSL has checked the server's certificate chain, its host name and the
   validity period of each certificate, at the clock's time when there is a
   clock. While the clock has only a lower bound of the time, a certificate
   that is not valid yet cannot be told from one that is: it may simply have
   been issued after the bound. That alone is accepted here. Everything else
   OpenSSL refused stands, a certificate that expired before the bound
   included.

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

static ebisu_sock_code_t
    openssl_connect(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    int sock, ret;
    SSL *ssl = NULL;
    ebisu_openssl_socket_t* ctx = EBISU_CONTAINER_OF(self, ebisu_openssl_socket_t, base);

    ebisu_sock_error_t error = EBISU_SOCK_ERROR_OTHER;
    long detail = 0;
    sock = connect_to_host(host, port, ctx->to_connect, ctx->logger, &error, &detail,
            ctx->last_address, &ctx->last_address_size);
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

    const tcp_options_t options = {
        ctx->keepalive_idle, ctx->keepalive_interval, ctx->keepalive_count, ctx->nodelay
    };
    set_tcp_options(sock, &options, ctx->logger);

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
       certificate has to be issued for, unless the configuration checks
       less. An address is no name: the server is told none, as RFC 6066 has
       it, and the certificate has to be issued for the address, among its
       subjectAltNames of that kind, which no name of the same text is. An
       address is what getaddrinfo() takes for one, its scope aside, which
       no certificate has. */
    const ebisu_tls_verify_t verify = verify_of(ssl_ctx, NULL);
    ebisu_dns_addr_t address;
    const bool is_address = ebisu_dns_parse_address(NULL, host, strlen(host), 0, true, &address);
    if (!is_address) {
        SSL_set_tlsext_host_name(ssl, host);
    }
    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (verify == EBISU_TLS_VERIFY_FULL &&
            (is_address ? X509_VERIFY_PARAM_set1_ip(param, address.bytes,
                    address.family == EBISU_DNS_IPV4 ? 4 : 16) :
                X509_VERIFY_PARAM_set1_host(param, host, 0)) != 1) {
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to set the host name to verify.");
        error = EBISU_SOCK_ERROR_OTHER;
        goto fail;
    }

    /* Checked at the clock's time, or the lower bound it has of it. Without a
       clock, at the system's. */
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

#ifdef SO_NOSIGPIPE
    {
        /* See sigpipe_guard_t. */
        int on = 1;
        setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
    }
#endif
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
    first_record_t first;
    memset(&first, 0, sizeof(first));
    SSL_set_msg_callback(ssl, note_first_record);
    SSL_set_msg_callback_arg(ssl, &first);
    sigpipe_guard_t guard;
    sigpipe_block(&guard);
    ret = handshake(ssl, sock, ctx->to_recv, ctx->to_send, ctx->to_connect);
    sigpipe_restore(&guard);
    SSL_set_msg_callback(ssl, NULL);
    SSL_set_msg_callback_arg(ssl, NULL);
    if (ret != 1) {
        /* errno first, before anything else can change it. */
        int saved_errno = errno;
        int ssl_error = SSL_get_error(ssl, ret);
        forget_session(ssl_ctx, host, port);
        long verified = SSL_get_verify_result(ssl);
        if (verified != X509_V_OK) {
            log_to(ctx->logger, EBISU_LOG_ERROR, "server certificate for %s rejected: %s",
                    host, verify == EBISU_TLS_VERIFY_PIN ?
                        "neither it nor its public key has the pin" :
                        X509_verify_cert_error_string(verified));
            error = EBISU_SOCK_ERROR_VERIFY;
            detail = verified;
        } else {
            /* A timeout, a peer that went, or TLS itself. */
            error = io_error(ssl_error, saved_errno, &detail);
            if (error == EBISU_SOCK_ERROR_OTHER || error == EBISU_SOCK_ERROR_TLS) {
                error = EBISU_SOCK_ERROR_TLS;
                detail = (long)ERR_peek_error();
                if (is_mismatch((unsigned long)detail, &first)) {
                    error = EBISU_SOCK_ERROR_TLS_MISMATCH;
                }
            }
            log_ssl_error(ctx->logger, "connect");
            if (error == EBISU_SOCK_ERROR_TLS_MISMATCH) {
                log_to(ctx->logger, EBISU_LOG_ERROR,
                        "%s may take only TLS older than 1.2, or TLS 1.2 without ECDHE or DHE "
                        "with AES-GCM or ChaCha20-Poly1305, which this port needs.", host);
            }
        }
        goto fail;
    }

    if (SSL_session_reused(ssl)) {
        log_to(ctx->logger, EBISU_LOG_DEBUG, "resumed the TLS session with %s.", host);
    }
    if (verify == EBISU_TLS_VERIFY_NONE) {
        log_to(ctx->logger, EBISU_LOG_WARNING,
                "connected to %s without verifying its certificate.", host);
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
    sigpipe_guard_t guard;
    sigpipe_block(&guard);
    int ret = SSL_write(ctx->ssl, buffer, length);
    sigpipe_restore(&guard);
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
    /* A read can write too: an alert, or a handshake the peer asked for. */
    sigpipe_guard_t guard;
    sigpipe_block(&guard);
    int ret = SSL_read(ctx->ssl, buffer, length_to_read);
    sigpipe_restore(&guard);
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
    if (ctx->ssl == NULL) {
        /* No connection: none made yet, the last connect failed, or the last
           connection is closed already. openssl_connect() sets ctx->ssl and
           ctx->socket only once connected. */
        return EBISU_SOCK_OK;
    }
    /* 1 means the bidirectional shutdown completed. 0 means our close_notify
       went out but the peer's has not arrived yet, which is not an error and is
       the normal outcome against a server that closes the connection itself.
       Everything below is torn down regardless, so a unidirectional shutdown is
       all we need. SSL_get_error() is only meaningful for a negative return.
       See https://www.openssl.org/docs/man3.0/man3/SSL_shutdown.html */
    int shutdown_ok = 1;
    if (!ctx->ssl_failed) {
        clear_errors();
        sigpipe_guard_t guard;
        sigpipe_block(&guard);
        int ret = SSL_shutdown(ctx->ssl);
        sigpipe_restore(&guard);
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
    ctx->ssl = NULL;
    ctx->socket = -1;
    if (!shutdown_ok) {
        log_to(ctx->logger, EBISU_LOG_ERROR, "failed to close.");
        return EBISU_SOCK_FAIL;
    }
    return EBISU_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

