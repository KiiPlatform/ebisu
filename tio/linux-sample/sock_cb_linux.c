#include "sock_cb_linux.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <errno.h>

/* Suppress warnings, because OpenSSL was deprecated in Mac. */
#ifdef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            unsigned int port)
{
    int sock, ret;
    struct hostent *servhost;
    struct sockaddr_in server;
    SSL *ssl = NULL;
    SSL_CTX *ssl_ctx = NULL;

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

    socket_context_t* ctx = (socket_context_t*)sock_ctx;
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
        return KHC_SOCK_FAIL;
    }

    SSL_library_init();
    const SSL_METHOD *method =
#if (OPENSSL_VERSION_NUMBER < 0x10100000L)
        TLSv1_2_client_method();
#else
        TLS_client_method();
#endif
    ssl_ctx = SSL_CTX_new(method);
    if (ssl_ctx == NULL){
        printf("failed to init ssl context.\n");
        return KHC_SOCK_FAIL;
    }

#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
    /* Servers that answer "Connection: close" drop the transport without
       sending a TLS close_notify. OpenSSL 1.1.1 surfaced that as
       SSL_ERROR_SYSCALL with errno 0, i.e. an ordinary EOF; 3.0 turned it into
       a hard "unexpected eof while reading" error instead. This option asks
       for the old, lenient behaviour. */
    SSL_CTX_set_options(ssl_ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);
#endif

    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL){
        printf("failed to init ssl.\n");
        return KHC_SOCK_FAIL;
    }

    ret = SSL_set_fd(ssl, sock);
    if (ret == 0){
        printf("failed to set fd.\n");
        return KHC_SOCK_FAIL;
    }

    ret = SSL_connect(ssl);
    if (ret != 1) {
        char sslErrStr[120];
        ERR_error_string_n(ERR_get_error(), sslErrStr, 120);
        printf("failed to connect: %s\n", sslErrStr);
        return KHC_SOCK_FAIL;
    }

    ctx->socket = sock;
    ctx->ssl = ssl;
    ctx->ssl_ctx = ssl_ctx;
    return KHC_SOCK_OK;
}

khc_sock_code_t
    sock_cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    int ret = SSL_write(ctx->ssl, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;
        return KHC_SOCK_OK;
    } else {
        printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t
    sock_cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    socket_context_t* ctx = (socket_context_t*)socket_context;
    *out_actual_length = 0;
    int ret = SSL_read(ctx->ssl, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;
        return KHC_SOCK_OK;
    } else if (ret == 0) {
        int ssl_error = SSL_get_error(ctx->ssl, ret);
        if (ssl_error == SSL_ERROR_ZERO_RETURN) {
            return KHC_SOCK_OK;
        } else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE) {
            return KHC_SOCK_AGAIN;
        } else if (ssl_error == SSL_ERROR_SYSCALL){
            if (errno == 0){
                return KHC_SOCK_OK;
            } else {
                printf("SSL_ERROR_SYSCALL: errno=%d: %s\n", errno, strerror(errno));
                return KHC_SOCK_FAIL;
            }
        } else {
            return KHC_SOCK_FAIL;
        }
        return KHC_SOCK_FAIL;
    } else {
        return KHC_SOCK_FAIL;
    }
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

