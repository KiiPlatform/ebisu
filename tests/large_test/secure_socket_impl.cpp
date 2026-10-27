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

namespace {

socket_context_t* linux_context(void* socket_context)
{
    return &((ebisu::ltest::ssl::SSLData*)socket_context)->linux_ctx;
}

}

// TLS goes through the Linux sample's callbacks, OpenSSL over BSD sockets, so
// the large tests run them against the live server: the server's certificate
// is checked at the tests' clock's time.
khc_sock_code_t
    ebisu::ltest::ssl::cb_connect(void* socket_context, const char* host,
            uint16_t port)
{
    return sock_cb_connect(linux_context(socket_context), host, port);
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    khc_sock_code_t res = sock_cb_send(linux_context(socket_context),
            buffer, length, out_sent_length);

#ifdef SOCKET_LOG
    if (res == KHC_SOCK_OK) {
        printf("%.*s", (int)*out_sent_length, buffer);
    }
#endif

    return res;
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    khc_sock_code_t res = sock_cb_recv(linux_context(socket_context),
            buffer, length_to_read, out_actual_length);

#ifdef SOCKET_LOG
    if (res == KHC_SOCK_OK) {
        printf("%.*s", (int)*out_actual_length, buffer);
    }
#endif

    return res;
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_close(void* socket_context)
{
    khc_sock_code_t res = sock_cb_close(linux_context(socket_context));

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return res;
}

/// raw
khc_sock_code_t
    ebisu::ltest::tcp::cb_connect(void* socket_context, const char* host,
            uint16_t port)
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
    ctx->linux_ctx.socket = sock;
    return KHC_SOCK_OK;
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu::ltest::ssl::SSLData* ctx = (ebisu::ltest::ssl::SSLData*)socket_context;
    int ret = send(ctx->linux_ctx.socket, buffer, length, 0);
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
    int ret = recv(ctx->linux_ctx.socket, buffer, length_to_read, 0);
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
    shutdown(ctx->linux_ctx.socket, SHUT_RDWR);
    // ignore close errors
    close(ctx->linux_ctx.socket);

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return KHC_SOCK_OK;
}

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
