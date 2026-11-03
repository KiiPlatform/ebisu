// The socket callbacks of secure_socket_impl.h on Windows. TLS goes through
// platform's Schannel port, Winsock and Schannel, so the large tests run it
// against the live server too. PLAIN_HTTP uses Winsock alone.

#include "secure_socket_impl.h"

#include <ws2tcpip.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

namespace {

// Winsock has to be started before the first socket is made, and the suites'
// main() is Catch's.
struct WinsockStartup {
    WinsockStartup() {
        WSADATA data;
        int err = WSAStartup(MAKEWORD(2, 2), &data);
        if (err != 0) {
            printf("WSAStartup failed: %d\n", err);
        }
    }
    ~WinsockStartup() {
        WSACleanup();
    }
} winsock_startup;

ebisu_schannel_socket_t* win32_context(ebisu_socket_t* self)
{
    return &ebisu::ltest::ssl::data_of(self)->platform;
}

}

static ebisu_sock_code_t
    cb_connect_ssl(ebisu_socket_t* self, const char* host,
            uint16_t port)
{
    return win32_context(self)->base.ops->connect(&win32_context(self)->base, host, port);
}

static ebisu_sock_code_t
    cb_send_ssl(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu_sock_code_t res = win32_context(self)->base.ops->send(&win32_context(self)->base,
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
    ebisu_sock_code_t res = win32_context(self)->base.ops->recv(&win32_context(self)->base,
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
    ebisu_sock_code_t res = win32_context(self)->base.ops->close(&win32_context(self)->base);

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
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned int)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    struct addrinfo* addrs = NULL;
    if (getaddrinfo(host, service, &hints, &addrs) != 0) {
        printf("failed to get host.\n");
        return EBISU_SOCK_FAIL;
    }

    // The first address that accepts, IPv6 or IPv4.
    SOCKET sock = INVALID_SOCKET;
    for (struct addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
        sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }
        if (connect(sock, addr->ai_addr, (int)addr->ai_addrlen) == 0) {
            break;
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    freeaddrinfo(addrs);
    if (sock == INVALID_SOCKET) {
        printf("failed to connect socket: %d\n", WSAGetLastError());
        return EBISU_SOCK_FAIL;
    }
    win32_context(self)->socket = sock;
    return EBISU_SOCK_OK;
}

static ebisu_sock_code_t
    cb_send_tcp(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    ebisu_schannel_socket_t* ctx = win32_context(self);
    int ret = send(ctx->socket, buffer,
            length > INT_MAX ? INT_MAX : (int)length, 0);
    if (ret > 0) {
        *out_sent_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return EBISU_SOCK_OK;
    } else {
        printf("failed to send: %d\n", WSAGetLastError());
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
    ebisu_schannel_socket_t* ctx = win32_context(self);
    int ret = recv(ctx->socket, buffer,
            length_to_read > INT_MAX ? INT_MAX : (int)length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return EBISU_SOCK_OK;
    } else if (ret == 0) {
        return EBISU_SOCK_OK;
    } else {
        printf("failed to receive: %d\n", WSAGetLastError());
        return EBISU_SOCK_FAIL;
    }
}

static ebisu_sock_code_t
    cb_close_tcp(ebisu_socket_t* self)
{
    ebisu_schannel_socket_t* ctx = win32_context(self);
    shutdown(ctx->socket, SD_BOTH);
    // ignore close errors
    closesocket(ctx->socket);
    ctx->socket = INVALID_SOCKET;

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
