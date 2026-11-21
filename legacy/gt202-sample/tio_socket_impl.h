#ifndef _tio_socket_impl_
#define _tio_socket_impl_

#include <ebisu_socket.h>

#include "main.h"

typedef struct
{
    /* What tio is given. */
    ebisu_socket_t base;
    SSL *ssl;
    int sock;
    unsigned int to_recv;
    unsigned int to_send;
    int show_debug;
} socket_context_t;

void ssl_ctx_init();

/* Clears the context, with the timeouts of the socket, in seconds, and makes
   its base a socket of the functions below: with sock_cb_recv() for HTTP, or
   mqtt_cb_recv() for MQTT. */
void sock_cb_init_http(socket_context_t* ctx, unsigned int to_recv, unsigned int to_send);
void sock_cb_init_mqtt(socket_context_t* ctx, unsigned int to_recv, unsigned int to_send);

void ssl_ctx_close();

/* HTTP Callback functions */
ebisu_sock_code_t sock_cb_connect(
        ebisu_socket_t* self,
        const char* host,
        uint16_t port);

ebisu_sock_code_t sock_cb_send(
        ebisu_socket_t* self,
        const char* buffer,
        size_t length,
        size_t* out_sent_length);

ebisu_sock_code_t sock_cb_recv(
        ebisu_socket_t* self,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);

ebisu_sock_code_t mqtt_cb_recv(
        ebisu_socket_t* self,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length);

ebisu_sock_code_t sock_cb_close(ebisu_socket_t* self);

void logger_cb(const char* format, ...);

#endif
