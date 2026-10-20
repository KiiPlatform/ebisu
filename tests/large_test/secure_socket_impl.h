#ifndef __KHC_CORE_SOCKET
#define __KHC_CORE_SOCKET

#include "khc_socket_callback.h"
#ifdef _WIN32
#include "sock_cb_win32.h"
#else
#include <openssl/ssl.h>
#endif

namespace ebisu {
namespace ltest {
namespace ssl {

// One connection, for the callbacks in both namespaces below.
struct SSLData
{
#ifdef _WIN32
    // The win32 sample's context: Winsock, with Schannel for TLS. A plain TCP
    // connection uses only its socket.
    socket_context_t win32;

    SSLData() {
        // No timeouts, as with OpenSSL.
        sock_cb_init_context(&win32, 0, 0);
    }
#else
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int socket;
#endif
};

khc_sock_code_t
    cb_connect(void* socket_context, const char* host,
            uint16_t port);

khc_sock_code_t
    cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

khc_sock_code_t
    cb_close(void* socket_context);

}

namespace tcp {
khc_sock_code_t
    cb_connect(void* socket_context, const char* host,
            uint16_t port);

khc_sock_code_t
    cb_send(void* socket_context,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    cb_recv(void* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length);

khc_sock_code_t
    cb_close(void* socket_context);
}

}
}

#endif /* __KHC_CORE_SOCKET */
