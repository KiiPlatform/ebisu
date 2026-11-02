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

ebisu_socket_t* sample_socket(ebisu_socket_t* self)
{
    return ebisu::ltest::ssl::data_of(self)->sample();
}

}

// TLS goes through the Linux sample's callbacks, OpenSSL over BSD sockets, so
// the large tests run them against the live server: the server's certificate
// is checked at the tests' clock's time.
static ebisu_sock_code_t
    cb_connect_ssl(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    return sock_cb_connect(sample_socket(self), host, port);
}

static ebisu_sock_code_t
    cb_send_ssl(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu_sock_code_t res = sock_cb_send(sample_socket(self),
            buffer, length, out_sent_length);

#ifdef SOCKET_LOG
    if (res == EBISU_SOCK_OK) {
        printf("%.*s", (int)*out_sent_length, buffer);
    }
#endif

    return res;
}

static ebisu_sock_code_t
    cb_recv_ssl(ebisu_socket_t* self,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    ebisu_sock_code_t res = sock_cb_recv(sample_socket(self),
            buffer, length_to_read, out_actual_length);

#ifdef SOCKET_LOG
    if (res == EBISU_SOCK_OK) {
        printf("%.*s", (int)*out_actual_length, buffer);
    }
#endif

    return res;
}

static ebisu_sock_code_t
    cb_close_ssl(ebisu_socket_t* self)
{
    ebisu_sock_code_t res = sock_cb_close(sample_socket(self));

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return res;
}

/// raw
static ebisu_sock_code_t
    cb_connect_tcp(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    int sock;
    struct hostent *servhost;
    struct sockaddr_in server;

    servhost = gethostbyname(host);
    if (servhost == NULL) {
        printf("failed to get host.\n");
        return EBISU_SOCK_FAIL;
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
        return EBISU_SOCK_FAIL;
    }

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) == -1 ){
        printf("failed to connect socket.\n");
        return EBISU_SOCK_FAIL;
    }
    ebisu::ltest::ssl::SSLData* ctx = ebisu::ltest::ssl::data_of(self);
    ctx->linux_ctx.socket = sock;
    return EBISU_SOCK_OK;
}

static ebisu_sock_code_t
    cb_send_tcp(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu::ltest::ssl::SSLData* ctx = ebisu::ltest::ssl::data_of(self);
    int ret = send(ctx->linux_ctx.socket, buffer, length, 0);
    if (ret > 0) {
        *out_sent_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return EBISU_SOCK_OK;
    } else {
        printf("failed to send\n");
        return EBISU_SOCK_FAIL;
    }
}

static ebisu_sock_code_t
    cb_recv_tcp(ebisu_socket_t* self,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    *out_actual_length = 0;
    ebisu::ltest::ssl::SSLData* ctx = ebisu::ltest::ssl::data_of(self);
    int ret = recv(ctx->linux_ctx.socket, buffer, length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return EBISU_SOCK_OK;
    } else if (ret == 0) {
      return EBISU_SOCK_OK;
    } else {
        return EBISU_SOCK_FAIL;
    }
}

static ebisu_sock_code_t
    cb_close_tcp(ebisu_socket_t* self)
{
    ebisu::ltest::ssl::SSLData* ctx = ebisu::ltest::ssl::data_of(self);
    shutdown(ctx->linux_ctx.socket, SHUT_RDWR);
    // ignore close errors
    close(ctx->linux_ctx.socket);

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return EBISU_SOCK_OK;
}

const ebisu_socket_ops_t ebisu::ltest::ssl::ops = {
    cb_connect_ssl, cb_send_ssl, cb_recv_ssl, cb_close_ssl, NULL
};

const ebisu_socket_ops_t ebisu::ltest::tcp::ops = {
    cb_connect_tcp, cb_send_tcp, cb_recv_tcp, cb_close_tcp, NULL
};

#ifdef __APPLE__
#pragma GCC diagnostic pop
#endif
