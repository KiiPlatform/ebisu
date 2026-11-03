#ifndef SOCK_CB_ESP32_H
#define SOCK_CB_ESP32_H

#include <ebisu_clock.h>
#include <ebisu_socket.h>
#include <stdbool.h>
#include <esp_tls.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* What tio is given. */
    ebisu_socket_t base;
    esp_tls_t* tls;
    /* Timeouts of the connection's receives and sends, in seconds, 0 for
       none. */
    unsigned int to_recv;
    unsigned int to_send;
    /* What the last read that read nothing returned, for
       sock_cb_recv_timed_out(). */
    int recv_ret;
    /* The time the server's certificate is checked at. See ebisu_clock_t. */
    ebisu_clock_t* clock;
    /* Why the last operation that failed did, for sock_cb_last_error(), and
       more of it. */
    ebisu_sock_error_t error;
    long error_detail;
} socket_context_t;

/* Clears the context, with the timeouts of the socket, in seconds, 0 for none,
   and the clock the server's certificate is checked at the time of. */
void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send,
        ebisu_clock_t* clock);

ebisu_sock_code_t
    sock_cb_connect(ebisu_socket_t* self, const char* host,
            uint16_t port);

ebisu_sock_code_t
    sock_cb_send(ebisu_socket_t* self,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

ebisu_sock_code_t
    sock_cb_recv(ebisu_socket_t* self, char* buffer, size_t length_to_read,
            size_t* out_actual_length);

bool
    sock_cb_recv_timed_out(ebisu_socket_t* self);

ebisu_sock_error_t
    sock_cb_last_error(ebisu_socket_t* self, long* out_detail);

ebisu_sock_code_t
    sock_cb_close(ebisu_socket_t* self);

#ifdef __cplusplus
}
#endif

#endif /* SOCK_CB_ESP32_H */
