#ifndef _tio_socket_impl_
#define _tio_socket_impl_

#include "khc_socket_callback.h"

#include "wiced.h"
#include "wiced_log.h"
#include "wiced_tls.h"

typedef struct _socket_context {
    wiced_tcp_socket_t socket;
    wiced_tls_context_t tls_context;
    wiced_packet_t *packet;
    int packet_offset;
    int show_debug;
} socket_context_t;

/* HTTP Callback functions */
khc_sock_code_t sock_cb_connect(
        void* sock_ctx,
        const char* host,
        unsigned int port);

khc_sock_code_t sock_cb_send(
        void* sock_ctx,
        const char* buffer,
        size_t length,
        size_t* out_sent_length);

khc_sock_code_t sock_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);

khc_sock_code_t mqtt_cb_recv(
        void* sock_ctx,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);

khc_sock_code_t sock_cb_close(void* sock_ctx);

#endif
