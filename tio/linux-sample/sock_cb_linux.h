#ifndef SOCK_CB_LINUX_H
#define SOCK_CB_LINUX_H

#include <khc_socket_callback.h>
#include <openssl/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A TLS connection, which verifies the server's certificate, and its host
   name, against the CA certificates the system trusts, or those in the file or
   directory SSL_CERT_FILE or SSL_CERT_DIR names.

   Writing to a connection the peer has reset raises SIGPIPE, which ends the
   process: an application using these ignores it, as example.c does. */
typedef struct {
    SSL *ssl;
    int socket;
    /* The receive and send timeouts in seconds, 0 for none, which the
       application sets. A connect waits as long as a send. */
    unsigned int to_recv;
    unsigned int to_send;
    /* Whether SSL_read() or SSL_write() ended the connection with an error,
       after which SSL_shutdown() must not be called. 1 or 0, set to 0 by
       sock_cb_connect(). */
    int ssl_failed;
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

#endif /* SOCK_CB_LINUX_H */
