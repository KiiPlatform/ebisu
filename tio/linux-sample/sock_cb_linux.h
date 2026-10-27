#ifndef __SOCK_CB_LINUX
#define __SOCK_CB_LINUX

#include <khc_socket_callback.h>
#include <kii_task_callback.h>
#include <kii.h>
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
    /* SSL_get_error() and errno after the last SSL_read() that read nothing,
       for sock_cb_recv_timed_out(). */
    int recv_ssl_error;
    int recv_errno;
    /* The time the server's certificate is checked at, or NULL for the
       system's. See kii_clock_t. */
    kii_clock_t* clock;
    /* Whether the clock had the time, rather than its floor only, when the
       connection was made. */
    kii_bool_t time_exact;
} socket_context_t;

/* Clears the context, with the timeouts of the socket, in seconds, 0 for none.
   Set clock after it to check the server's certificate at its time. */
void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send);

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
    sock_cb_close(void* sock_context);

#ifdef __cplusplus
}
#endif

#endif /* __SOCK_CB_LINUX */
