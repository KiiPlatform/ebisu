#ifndef __SOCK_CB_LINUX
#define __SOCK_CB_LINUX

#include <ebisu_clock.h>
#include <ebisu_socket.h>
#include <openssl/ssl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* What tio is given. */
    ebisu_socket_t base;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int socket;
    unsigned int to_recv;
    unsigned int to_send;
    /* SSL_get_error() and errno after the last SSL_read() that read nothing,
       for sock_cb_recv_timed_out(). */
    int recv_ssl_error;
    int recv_errno;
    /* Whether SSL_read() or SSL_write() ended the connection with an error,
       after which SSL_shutdown() must not be called. */
    bool ssl_failed;
    /* The time the server's certificate is checked at, or NULL for the
       system's. See ebisu_clock_t. */
    ebisu_clock_t* clock;
    /* Whether the clock had the time, rather than its floor only, when the
       connection was made. */
    bool time_exact;
} socket_context_t;

/* Clears the context, with the timeouts of the socket, in seconds, 0 for none,
   and makes its base a socket of the functions below. Set clock after it to
   check the server's certificate at its time. */
void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send);

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

ebisu_sock_code_t
    sock_cb_close(ebisu_socket_t* self);

#ifdef __cplusplus
}
#endif

#endif /* __SOCK_CB_LINUX */
