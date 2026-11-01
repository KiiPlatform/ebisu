#include "sock_cb_linux.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <errno.h>

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

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->socket = -1;
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
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
    socket_context_t* ctx = (socket_context_t*)SSL_get_app_data(ssl);
    if (X509_STORE_CTX_get_error(store) == X509_V_ERR_CERT_NOT_YET_VALID &&
            ctx->clock != NULL) {
        if (ctx->time_exact != KII_TRUE) {
            return 1;
        }
        kii_clock_forget_time(ctx->clock);
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

static void print_ssl_failure(const char* what, int ssl_error, int saved_errno)
{
    if (ssl_error == SSL_ERROR_SYSCALL) {
        printf("failed to %s: errno=%d: %s\n", what, saved_errno, strerror(saved_errno));
    } else if (ssl_error == SSL_ERROR_SSL) {
        char sslErrStr[120];
        ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
        printf("failed to %s: %s\n", what, sslErrStr);
    } else {
        printf("failed to %s: SSL_get_error()=%d\n", what, ssl_error);
    }
}

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            uint16_t port)
{
    int sock, ret;
    struct hostent *servhost;
    struct sockaddr_in server;
    SSL *ssl = NULL;
    SSL_CTX *ssl_ctx = NULL;
    socket_context_t* ctx = (socket_context_t*)sock_ctx;

    servhost = gethostbyname(host);
    if (servhost == NULL) {
        printf("failed to get host.\n");
        return KHC_SOCK_FAIL;
    }
    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    /* More secure. */
    memcpy(&(server.sin_addr), servhost->h_addr, servhost->h_length);

    /* Get Port number */
    server.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        return KHC_SOCK_FAIL;
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

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) == -1 ){
        printf("failed to connect socket.\n");
        goto fail;
    }

    /* The tasks connect at the same time, and it is not safe to run twice. */
    static pthread_once_t openssl_initialized = PTHREAD_ONCE_INIT;
    pthread_once(&openssl_initialized, init_openssl);
    const SSL_METHOD *method =
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        TLSv1_2_client_method();
#else
        TLS_client_method();
#endif
    ssl_ctx = SSL_CTX_new(method);
    if (ssl_ctx == NULL){
        printf("failed to init ssl context.\n");
        goto fail;
    }

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    /* Servers that answer "Connection: close" drop the transport without
       sending a TLS close_notify. OpenSSL 1.1.1 surfaced that as
       SSL_ERROR_SYSCALL with errno 0, i.e. an ordinary EOF; 3.0 turned it into
       a hard "unexpected eof while reading" error instead. This option asks
       for the old, lenient behaviour. */
    SSL_CTX_set_options(ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    /* The server's certificate has to chain to one of the system's CA
       certificates. */
    if (SSL_CTX_set_default_verify_paths(ssl_ctx) != 1) {
        printf("failed to load the CA certificates.\n");
        goto fail;
    }
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, verify_callback);

    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL){
        printf("failed to init ssl.\n");
        goto fail;
    }
    SSL_set_app_data(ssl, ctx);

    /* The name the server is to present a certificate for, and the one the
       certificate has to be issued for. */
    SSL_set_tlsext_host_name(ssl, host);
    X509_VERIFY_PARAM* param = SSL_get0_param(ssl);
    X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (X509_VERIFY_PARAM_set1_host(param, host, 0) != 1) {
        printf("failed to set the host name to verify.\n");
        goto fail;
    }

    /* Checked at the clock's time: its floor until a server has stated the
       time, that time from then on. Without a clock, at the system's. */
    if (ctx->clock != NULL) {
        int64_t now = 0;
        ctx->time_exact = kii_clock_get_time(ctx->clock, &now);
        /* A 32-bit time_t ends in 2038. Cut down to fit one, the time would
           be decades out, and certificates refused for what they are not. */
        if ((int64_t)(time_t)now != now) {
            printf("the time does not fit in a time_t.\n");
            goto fail;
        }
        X509_VERIFY_PARAM_set_time(param, (time_t)now);
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret == 0){
        printf("failed to set fd.\n");
        goto fail;
    }

    clear_errors();
    ret = SSL_connect(ssl);
    if (ret != 1) {
        long verified = SSL_get_verify_result(ssl);
        if (verified != X509_V_OK) {
            printf("server certificate for %s rejected: %s\n", host,
                    X509_verify_cert_error_string(verified));
        } else {
            char sslErrStr[120];
            ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
            printf("failed to connect: %s\n", sslErrStr);
        }
        goto fail;
    }

    ctx->socket = sock;
    ctx->ssl = ssl;
    ctx->ssl_ctx = ssl_ctx;
    ctx->ssl_failed = KII_FALSE;
    return KHC_SOCK_OK;

fail:
    if (ssl != NULL) {
        SSL_free(ssl);
    }
    if (ssl_ctx != NULL) {
        SSL_CTX_free(ssl_ctx);
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
    clear_errors();
    int ret = SSL_write(ctx->ssl, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return KHC_SOCK_OK;
    }
    /* errno first, before anything else can change it. */
    int saved_errno = errno;
    int ssl_error = SSL_get_error(ctx->ssl, ret);
    if (ssl_error == SSL_ERROR_SYSCALL || ssl_error == SSL_ERROR_SSL) {
        ctx->ssl_failed = KII_TRUE;
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
    clear_errors();
    int ret = SSL_read(ctx->ssl, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return KHC_SOCK_OK;
    }
    /* errno first, before anything else can change it. */
    ctx->recv_errno = errno;
    ctx->recv_ssl_error = SSL_get_error(ctx->ssl, ret);
    switch (ctx->recv_ssl_error) {
    case SSL_ERROR_ZERO_RETURN:
        return KHC_SOCK_OK;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        /* The receive timeout, as these sockets block. Not KHC_SOCK_AGAIN,
           on which khc reads again at once, and for good if the server has
           gone. See sock_cb_recv_timed_out(). */
        return KHC_SOCK_FAIL;
    case SSL_ERROR_SYSCALL:
        ctx->ssl_failed = KII_TRUE;
        /* The end of the stream, from a peer that sent no close_notify. */
        if (ctx->recv_errno == 0) {
            return KHC_SOCK_OK;
        }
        break;
    case SSL_ERROR_SSL:
        ctx->ssl_failed = KII_TRUE;
        break;
    default:
        break;
    }
    print_ssl_failure("receive", ctx->recv_ssl_error, ctx->recv_errno);
    return KHC_SOCK_FAIL;
}

/* sock_cb_recv() returns KHC_SOCK_FAIL both when the receive timeout set with
   SO_RCVTIMEO in sock_cb_connect() runs out and when the connection fails.
   The timeout makes the read fail with EAGAIN or EWOULDBLOCK, which OpenSSL
   reports as SSL_ERROR_WANT_READ, and errno is only meaningful with
   SSL_ERROR_SYSCALL. */
kii_bool_t
    sock_cb_recv_timed_out(void* socket_context)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    if (ctx->recv_ssl_error == SSL_ERROR_WANT_READ) {
        return KII_TRUE;
    }
    if (ctx->recv_ssl_error == SSL_ERROR_SYSCALL &&
            (ctx->recv_errno == EAGAIN || ctx->recv_errno == EWOULDBLOCK)) {
        return KII_TRUE;
    }
    return KII_FALSE;
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
       See https://www.openssl.org/docs/man3.0/man3/SSL_shutdown.html */
    int shutdown_ok = 1;
    if (ctx->ssl_failed != KII_TRUE) {
        clear_errors();
        int ret = SSL_shutdown(ctx->ssl);
        if (ret < 0) {
            int sslErr = SSL_get_error(ctx->ssl, ret);
            if (sslErr != SSL_ERROR_SYSCALL && sslErr != SSL_ERROR_ZERO_RETURN) {
                char sslErrStr[120];
                ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
                printf("failed to shutdown: %s\n", sslErrStr);
                shutdown_ok = 0;
            }
            /* Otherwise the peer closed the transport without a close_notify,
               which is harmless while we are tearing the connection down. */
        }
    }
    close(ctx->socket);
    SSL_free(ctx->ssl);
    SSL_CTX_free(ctx->ssl_ctx);
    if (!shutdown_ok) {
        printf("failed to close:\n");
        return KHC_SOCK_FAIL;
    }
    return KHC_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif

