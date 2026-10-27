#ifndef SOCK_CB_ESP32_H
#define SOCK_CB_ESP32_H

#include <khc_socket_callback.h>
#include <kii.h>
#include <esp_tls.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    esp_tls_t* tls;
    /* Timeouts of the connection's receives and sends, in seconds, 0 for
       none. */
    unsigned int to_recv;
    unsigned int to_send;
    /* What the last read that read nothing returned, for
       sock_cb_recv_timed_out(). */
    int recv_ret;
    /* The time the server's certificate is checked at. See kii_clock_t. */
    kii_clock_t* clock;
} socket_context_t;

/* Clears the context, with the timeouts of the socket, in seconds, 0 for none,
   and the clock the server's certificate is checked at the time of. */
void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send,
        kii_clock_t* clock);

khc_sock_code_t
    sock_cb_connect(void* sock_ctx, const char* host,
            uint16_t port);

khc_sock_code_t
    sock_cb_send(void* sock_ctx,
            const char* buffer,
            size_t length,
            size_t* out_sent_length);

khc_sock_code_t
    sock_cb_recv(void* sock_ctx, char* buffer, size_t length_to_read,
            size_t* out_actual_length);

kii_bool_t
    sock_cb_recv_timed_out(void* sock_ctx);

khc_sock_code_t
    sock_cb_close(void* sock_ctx);

#ifdef __cplusplus
}
#endif

#endif /* SOCK_CB_ESP32_H */
