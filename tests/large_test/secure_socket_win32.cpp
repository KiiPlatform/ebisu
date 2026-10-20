// The socket callbacks of secure_socket_impl.h on Windows. TLS goes through the
// win32 sample's callbacks, Winsock and Schannel, so the large tests run them
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

socket_context_t* win32_context(void* socket_context)
{
    return &((ebisu::ltest::ssl::SSLData*)socket_context)->win32;
}

}

khc_sock_code_t
    ebisu::ltest::ssl::cb_connect(void* socket_context, const char* host,
            uint16_t port)
{
    return sock_cb_connect(win32_context(socket_context), host, port);
}

khc_sock_code_t
    ebisu::ltest::ssl::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    khc_sock_code_t res = sock_cb_send(win32_context(socket_context),
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
    khc_sock_code_t res = sock_cb_recv(win32_context(socket_context),
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
    khc_sock_code_t res = sock_cb_close(win32_context(socket_context));

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
        return KHC_SOCK_FAIL;
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
        return KHC_SOCK_FAIL;
    }
    win32_context(socket_context)->socket = sock;
    return KHC_SOCK_OK;
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length)
{
    socket_context_t* ctx = win32_context(socket_context);
    int ret = send(ctx->socket, buffer,
            length > INT_MAX ? INT_MAX : (int)length, 0);
    if (ret > 0) {
        *out_sent_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return KHC_SOCK_OK;
    } else {
        printf("failed to send: %d\n", WSAGetLastError());
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
    socket_context_t* ctx = win32_context(socket_context);
    int ret = recv(ctx->socket, buffer,
            length_to_read > INT_MAX ? INT_MAX : (int)length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;

#ifdef SOCKET_LOG
        printf("%.*s", ret, buffer);
#endif

        return KHC_SOCK_OK;
    } else if (ret == 0) {
        return KHC_SOCK_OK;
    } else {
        printf("failed to receive: %d\n", WSAGetLastError());
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t
    ebisu::ltest::tcp::cb_close(void* socket_context)
{
    socket_context_t* ctx = win32_context(socket_context);
    shutdown(ctx->socket, SD_BOTH);
    // ignore close errors
    closesocket(ctx->socket);
    ctx->socket = INVALID_SOCKET;

#ifdef SOCKET_LOG
    printf("\n----------------------------------------------------------------------------------------------------\n\n\n");
#endif

    return KHC_SOCK_OK;
}
