#ifndef _gt202_kii_adapter
#define _gt202_kii_adapter

#include "kii.h"

#include "main.h"

typedef enum prv_state {
    PRV_STATE_IDLE,
    PRV_STATE_CONNECT,
    PRV_STATE_SEND,
    PRV_STATE_RECV,
    PRV_STATE_CLOSE,
} prv_state_t;

typedef struct context
{
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int sock;
    char* buff;
    size_t buff_size;
    char host[256];
    prv_state_t state;
    int last_chunk;
    int sent_size;
    int received_size;
} context_t;

/* HTTP Callback functions */
kii_socket_code_t socket_connect_cb(
        kii_socket_context_t* socket_context,
        const char* host,
        unsigned int port);

kii_socket_code_t socket_send_cb(
        kii_socket_context_t* socket_context,
        const char* buffer,
        size_t length);

kii_socket_code_t socket_recv_cb(
        kii_socket_context_t* socket_context,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);

kii_socket_code_t socket_close_cb(kii_socket_context_t* socket_context);

void logger_cb(const char* format, ...);

#endif
