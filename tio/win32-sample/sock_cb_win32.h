#ifndef SOCK_CB_WIN32_H
#define SOCK_CB_WIN32_H

/* Socket callbacks for khc and kii on Windows: Winsock 2 for the TCP
   connection, and Schannel, through SSPI, for TLS. */

#include <winsock2.h>
#define SECURITY_WIN32
#include <security.h>

#include <khc_socket_callback.h>
#include <kii_task_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One connection. Initialise it with sock_cb_init_context(), then pass it as
   the context of all of the callbacks below; a context serves one connection
   at a time, and is reused for the next one after sock_cb_close(). */
typedef struct {
    /* Receive and send timeouts in seconds; 0 waits for ever. */
    unsigned int to_recv;
    unsigned int to_send;

    /* The rest belongs to the callbacks. */
    /* The server's name, which its certificate must be issued for. */
    char host[256];
    SOCKET socket;
    CredHandle cred;
    int has_cred;
    CtxtHandle ctxt;
    int has_ctxt;
    SecPkgContext_StreamSizes sizes;

    /* What has been received from the peer and not yet returned. recv_buf
       holds TLS records as they came off the socket, recv_len bytes of them.
       DecryptMessage() decrypts a record in place, so once one has been
       decrypted its plaintext, the plain_len bytes at plain, lies inside the
       first record_len bytes of recv_buf, and anything after those is the
       start of the next record. */
    char* recv_buf;
    size_t recv_len;
    size_t record_len;
    char* plain;
    size_t plain_len;
    /* The peer ended the stream: TLS close_notify, or the end of the TCP
       stream at a record boundary. */
    int peer_closed;
    /* Whether the last sock_cb_recv() failed because nothing arrived within
       to_recv, for sock_cb_recv_timed_out(). */
    int recv_timed_out;

    /* A record being encrypted: header, at most cbMaximumMessage bytes of
       data, and trailer. */
    char* send_buf;
} socket_context_t;

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
    sock_cb_close(void* sock_ctx);

#ifdef __cplusplus
}
#endif

#endif /* SOCK_CB_WIN32_H */
