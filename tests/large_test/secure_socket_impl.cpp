#include "secure_socket_impl.h"

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
    ebisu::ltest::ssl::cb_connect(void* sock_ctx, const char* host,
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
        int sslErr= SSL_get_error(ssl, ret);
        char sslErrStr[120];
        ERR_error_string_n(sslErr, sslErrStr, 120);
        printf("failed to connect: %s\n", sslErrStr);
        return KHC_SOCK_FAIL;
    }

    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)sock_ctx;
    ctx->socket = sock;
    ctx->ssl = ssl;
    ctx->ssl_ctx = ssl_ctx;
    return KHC_SOCK_OK;
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    int ret = SSL_write(ctx->ssl, buffer, length);
    if (ret > 0) {
        *out_sent_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return KHC_SOCK_OK;
    } else {
        printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    *out_actual_length = 0;
    int ret = SSL_read(ctx->ssl, buffer, length_to_read);
    if (ret > 0) {
        *out_actual_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

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
    ebisu::ltest::ssl::cb_close(void* socket_context)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    int ret = SSL_shutdown(ctx->ssl);
    if (ret != 1) {
        int sslErr = SSL_get_error(ctx->ssl, ret);
        if (sslErr == SSL_ERROR_SYSCALL) {
            /* This is OK.*/
            /* See https://www.openssl.org/docs/ssl/SSL_shutdown.html */
            ret = 1;
        } else {
            char sslErrStr[120];
            ERR_error_string_n(sslErr, sslErrStr, 120);
            printf("failed to shutdown: %s\n", sslErrStr);
        }
    }
    close(ctx->socket);
    SSL_free(ctx->ssl);
    SSL_CTX_free(ctx->ssl_ctx);

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    if (ret != 1) {
        printf("failed to close:\n");
        return KHC_SOCK_FAIL;
    }
    return KHC_SOCK_OK;
}

/// raw
khc_sock_code_t
    ebisu::ltest::tcp::cb_connect(void* socket_context, const char* host,
            unsigned int port)
{
    int sock;
    struct hostent *servhost;
    struct sockaddr_in server;

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

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) == -1 ){
        printf("failed to connect socket.\n");
        return KHC_SOCK_FAIL;
    }
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    ctx->socket = sock;
    return KHC_SOCK_OK;
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    int ret = send(ctx->socket, buffer, length, 0);
    if (ret > 0) {
        *out_sent_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return KHC_SOCK_OK;
    } else {
        printf("failed to send\n");
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    *out_actual_length = 0;
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    int ret = recv(ctx->socket, buffer, length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return KHC_SOCK_OK;
    } else if (ret == 0) {
      return KHC_SOCK_OK;
    } else {
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_close(void* socket_context)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    shutdown(ctx->socket, SHUT_RDWR);
    // ignore close errors
    close(ctx->socket);

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return KHC_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
