#ifndef __SOCK_CB_LINUX
#define __SOCK_CB_LINUX

#include <khc_socket_callback.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int socket;
    unsigned int to_recv;
    unsigned int to_send;
} socket_context_t;

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            unsigned int port);

khc_sock_code_t
    sock_cb_send(void* sock_ctx,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    sock_cb_recv(void* sock_ctx, char* buffer, size_t length_to_read,
            size_t* out_actual_length);

khc_sock_code_t
    sock_cb_close(void* sock_context);

#ifdef __cplusplus
}
#endif

#endif /* __SOCK_CB_LINUX */
