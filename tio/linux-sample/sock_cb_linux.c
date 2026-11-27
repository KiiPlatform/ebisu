#include "sock_cb_linux.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

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

/* What SSL_get_error() answers depends on this thread's error queue being empty
   before the call it is asked about, and errno says something only if that call
   set it. A failure earlier on the thread leaves both behind: the task making a
   request is often the one whose last connection failed. */
static void clear_errors(void)
{
    ERR_clear_error();
    errno = 0;
}

static void print_ssl_failure(const char* what, int ssl_error, int saved_errno)
{
    if (ssl_error == SSL_ERROR_SYSCALL && saved_errno == 0) {
        printf("failed to %s: the connection was closed\n", what);
    } else if (ssl_error == SSL_ERROR_SYSCALL) {
        printf("failed to %s: errno=%d: %s\n", what, saved_errno, strerror(saved_errno));
    } else if (ssl_error == SSL_ERROR_SSL) {
        char sslErrStr[120];
        ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
        printf("failed to %s: %s\n", what, sslErrStr);
    } else {
        printf("failed to %s: SSL_get_error()=%d\n", what, ssl_error);
    }
}

/* The one all connections share: the CA certificates are loaded once. NULL if
   it couldn't be made. */
static SSL_CTX* shared_ssl_ctx = NULL;

static SSL_CTX* new_ssl_ctx(void)
{
    const SSL_METHOD *method =
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        TLSv1_2_client_method();
#else
        TLS_client_method();
#endif
    SSL_CTX* ssl_ctx = SSL_CTX_new(method);
    if (ssl_ctx == NULL) {
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

    /* The server's certificate has to chain to a CA the system trusts, or to
       one in the file or directory SSL_CERT_FILE or SSL_CERT_DIR names. */
    if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    return ssl_ctx;
}

static void init_openssl(void)
{
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
    for (int i = 0; i < CRYPTO_NUM_LOCKS; ++i) {
        pthread_mutex_init(&openssl_locks[i], NULL);
    }
    CRYPTO_set_locking_callback(openssl_lock);
    SSL_library_init();
    SSL_load_error_strings();
#endif
    shared_ssl_ctx = new_ssl_ctx();
}

static int64_t monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* When a timeout of seconds, started now, runs out, as wait_for() takes it: 0,
   for ever, for none. */
static int64_t deadline_in(unsigned int seconds)
{
    return seconds > 0 ? monotonic_ms() + (int64_t)seconds * 1000 : 0;
}

/* Waits for fd to be ready for events until deadline_ms, or for ever if it is
   0, through signals. 1 when it is, 0 when the deadline passed, -1 on
   failure. */
static int wait_for(int fd, short events, int64_t deadline_ms)
{
    for (;;) {
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

/* After an OpenSSL call wanted to read or write again on this blocking socket,
   for its timeout having run out, or for a signal: a socket with a timeout
   fails with EINTR on one even with SA_RESTART, and OpenSSL takes it for a
   call to try again. Whether the socket is ready before the deadline for the
   call to be made again. */
static int ready_before_deadline(int sock, int ssl_error, int64_t deadline_ms)
{
    if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE) {
        return 0;
    }
    return wait_for(sock, ssl_error == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT,
            deadline_ms) == 1;
}

/* A socket connected to ai, waiting no longer than timeout_s, 0 for ever, or
   -1. */
static int connect_to_address(const struct addrinfo* ai, unsigned int timeout_s)
{
    /* Not inherited by a program the application runs while the connection is
       open, which would hold it open past close(): at once where socket() can,
       so that no fork() in another thread comes between. */
#ifdef SOCK_CLOEXEC
    int sock = socket(ai->ai_family, ai->ai_socktype | SOCK_CLOEXEC, ai->ai_protocol);
#else
    int sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sock >= 0 && fcntl(sock, F_SETFD, FD_CLOEXEC) == -1) {
        close(sock);
        sock = -1;
    }
#endif
    if (sock < 0) {
        return -1;
    }
    /* Without blocking, to wait no longer than the timeout, through
       signals. */
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(sock);
        return -1;
    }
    int connected = (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0);
    if (!connected && errno == EINPROGRESS &&
            wait_for(sock, POLLOUT, deadline_in(timeout_s)) == 1) {
        int error = 0;
        socklen_t length = sizeof(error);
        connected = (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &length) == 0 &&
                error == 0);
    }
    /* Blocking again, for the handshake and what follows, which the receive
       and send timeouts bound. */
    if (!connected || fcntl(sock, F_SETFL, flags) != 0) {
        close(sock);
        return -1;
    }
    return sock;
}

/* A socket connected to one of host's addresses, IPv4 or IPv6, tried in the
   order the resolver gives them, each for timeout_s, 0 for ever. Or -1. */
static int connect_to_host(const char* host, unsigned int port, unsigned int timeout_s)
{
    char service[12];
    snprintf(service, sizeof(service), "%u", port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    /* Unlike gethostbyname(), safe in tasks that connect at the same time. */
    struct addrinfo* addrs = NULL;
    int error = getaddrinfo(host, service, &hints, &addrs);
    if (error != 0) {
        printf("failed to look up %s: %s\n", host, gai_strerror(error));
        return -1;
    }
    int sock = -1;
    for (const struct addrinfo* ai = addrs; ai != NULL && sock < 0; ai = ai->ai_next) {
        sock = connect_to_address(ai, timeout_s);
    }
    freeaddrinfo(addrs);
    if (sock < 0) {
        printf("failed to connect to %s.\n", host);
    }
    return sock;
}

/* The name the server is to present a certificate for, and the one the
   certificate has to be issued for. An address is no name: the server is told
   none, as RFC 6066 has it, and the certificate has to be issued for the
   address. 1 if it could be set. */
static int set_host(SSL* ssl, const char* host)
{
    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
    unsigned char address[16];
    if (inet_pton(AF_INET, host, address) == 1 || inet_pton(AF_INET6, host, address) == 1) {
        return X509_VERIFY_PARAM_set1_ip_asc(param, host);
    }
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (SSL_set_tlsext_host_name(ssl, host) != 1) {
        return 0;
    }
    return X509_VERIFY_PARAM_set1_host(param, host, 0);
}

static void set_timeout(int sock, int option, unsigned int seconds)
{
    if (seconds > 0) {
        struct timeval tv;
        tv.tv_sec = seconds;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, option, (const char*)&tv, sizeof tv);
    }
}

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            unsigned int port)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    /* The tasks connect at the same time, and it is not safe to run twice. */
    static pthread_once_t openssl_initialized = PTHREAD_ONCE_INIT;
    pthread_once(&openssl_initialized, init_openssl);
    if (shared_ssl_ctx == NULL) {
        printf("failed to init ssl context.\n");
        return KHC_SOCK_FAIL;
    }

    /* As long as a send may take, as Linux bounds a connect by the send
       timeout. */
    int sock = connect_to_host(host, port, ctx->to_send);
    if (sock < 0) {
        return KHC_SOCK_FAIL;
    }
    set_timeout(sock, SO_RCVTIMEO, ctx->to_recv);
    set_timeout(sock, SO_SNDTIMEO, ctx->to_send);

    SSL* ssl = SSL_new(shared_ssl_ctx);
    if (ssl == NULL) {
        printf("failed to init ssl.\n");
        goto fail;
    }
    if (set_host(ssl, host) != 1) {
        printf("failed to set the host to verify.\n");
        goto fail;
    }
    if (SSL_set_fd(ssl, sock) != 1) {
        printf("failed to set fd.\n");
        goto fail;
    }

    /* Each read and write bounded by its timeout, and after a signal by what
       is left of it, from the start. */
    const int64_t recv_deadline_ms = deadline_in(ctx->to_recv);
    const int64_t send_deadline_ms = deadline_in(ctx->to_send);
    int ret;
    int saved_errno = 0;
    int ssl_error = SSL_ERROR_NONE;
    for (;;) {
        clear_errors();
        ret = SSL_connect(ssl);
        if (ret == 1) {
            break;
        }
        /* errno first, before anything else can change it. */
        saved_errno = errno;
        ssl_error = SSL_get_error(ssl, ret);
        if (!ready_before_deadline(sock, ssl_error, ssl_error == SSL_ERROR_WANT_READ ?
                    recv_deadline_ms : send_deadline_ms)) {
            break;
        }
    }
    if (ret != 1) {
        long verified = SSL_get_verify_result(ssl);
        if (verified != X509_V_OK) {
            printf("server certificate for %s rejected: %s\n", host,
                    X509_verify_cert_error_string(verified));
        } else {
            print_ssl_failure("connect", ssl_error, saved_errno);
        }
        goto fail;
    }

    ctx->socket = sock;
    ctx->ssl = ssl;
    ctx->ssl_failed = 0;
    return KHC_SOCK_OK;

fail:
    if (ssl != NULL) {
        SSL_free(ssl);
    }
    close(sock);
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    /* What an int can't count is left to the next call. */
    const int to_write = length > INT_MAX ? INT_MAX : (int)length;
    const int64_t deadline_ms = deadline_in(ctx->to_send);
    int ret;
    int saved_errno = 0;
    int ssl_error = SSL_ERROR_NONE;
    for (;;) {
        clear_errors();
        /* Made again, after a signal, with what it was made with, as OpenSSL
           has it. */
        ret = SSL_write(ctx->ssl, buffer, to_write);
        if (ret > 0) {
            *out_sent_length = ret;
            return KHC_SOCK_OK;
        }
        /* errno first, before anything else can change it. */
        saved_errno = errno;
        ssl_error = SSL_get_error(ctx->ssl, ret);
        if (!ready_before_deadline(ctx->socket, ssl_error, deadline_ms)) {
            break;
        }
    }
    if (ssl_error == SSL_ERROR_SYSCALL || ssl_error == SSL_ERROR_SSL) {
        ctx->ssl_failed = 1;
    }
    print_ssl_failure("send", ssl_error, saved_errno);
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    *out_actual_length = 0;
    const int to_read = length_to_read > INT_MAX ? INT_MAX : (int)length_to_read;
    const int64_t deadline_ms = deadline_in(ctx->to_recv);
    int ret;
    int saved_errno = 0;
    int ssl_error = SSL_ERROR_NONE;
    for (;;) {
        clear_errors();
        ret = SSL_read(ctx->ssl, buffer, to_read);
        if (ret > 0) {
            *out_actual_length = ret;
            return KHC_SOCK_OK;
        }
        /* errno first, before anything else can change it. */
        saved_errno = errno;
        ssl_error = SSL_get_error(ctx->ssl, ret);
        if (!ready_before_deadline(ctx->socket, ssl_error, deadline_ms)) {
            break;
        }
    }
    switch (ssl_error) {
    case SSL_ERROR_ZERO_RETURN:
        return KHC_SOCK_OK;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* The receive timeout, as these sockets block. Not KHC_SOCK_AGAIN,
           on which khc reads again at once, and for good if the server has
           gone. */
        return KHC_SOCK_FAIL;
    case SSL_ERROR_SYSCALL:
        ctx->ssl_failed = 1;
        /* The end of the stream, from a peer that sent no close_notify. */
        if (saved_errno == 0) {
            return KHC_SOCK_OK;
        }
        break;
    case SSL_ERROR_SSL:
        ctx->ssl_failed = 1;
        break;
    default:
        break;
    }
    print_ssl_failure("receive", ssl_error, saved_errno);
    return KHC_SOCK_FAIL;
}

khc_sock_code_t
    sock_cb_close(void* socket_context)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    /* 1 means the bidirectional shutdown completed. 0 means our close_notify
       went out but the peer's has not arrived yet, which is not an error and is
       the normal outcome against a server that closes the connection itself.
       Everything below is torn down regardless, so a unidirectional shutdown is
       all we need. SSL_get_error() is only meaningful for a negative return.
       It must not be called at all on a connection that already failed, as
       OpenSSL says, and fails there.
       See https://www.openssl.org/docs/man3.0/man3/SSL_shutdown.html */
    int shutdown_ok = 1;
    if (ctx->ssl_failed == 0) {
        clear_errors();
        int ret = SSL_shutdown(ctx->ssl);
        if (ret < 0) {
            /* errno first, before anything else can change it. */
            int saved_errno = errno;
            int sslErr = SSL_get_error(ctx->ssl, ret);
            if (sslErr != SSL_ERROR_SYSCALL && sslErr != SSL_ERROR_ZERO_RETURN) {
                print_ssl_failure("shutdown", sslErr, saved_errno);
                shutdown_ok = 0;
            }
            /* Otherwise the peer closed the transport without a close_notify,
               which is harmless while we are tearing the connection down. */
        }
    }
    SSL_free(ctx->ssl);
    ctx->ssl = NULL;
    close(ctx->socket);
    ctx->socket = -1;
    if (!shutdown_ok) {
        printf("failed to close:\n");
        return KHC_SOCK_FAIL;
    }
    return KHC_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
