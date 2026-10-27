#ifndef SOCK_CB_FREERTOS_H
#define SOCK_CB_FREERTOS_H

#include <khc_socket_callback.h>
#include <kii.h>

#include <FreeRTOS.h>
#include <FreeRTOS_Sockets.h>
#include <mbedtls/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    Socket_t socket;
    mbedtls_ssl_context ssl;
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

/* Sets up the TLS configuration the connections share, with the CA
   certificates the server's has to chain to, PEM, with its terminating NUL.
   Once, before the first connection. 0 on success, or an mbedTLS error. */
int sock_cb_init_tls(const char* ca_pem, size_t ca_pem_size);

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

#endif /* SOCK_CB_FREERTOS_H */
