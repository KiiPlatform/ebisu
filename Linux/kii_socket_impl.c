#include "kii_socket_impl.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

typedef struct linux_socket_context_t {
    int sock
} linux_socket_context_t;

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port)
{
    int sock, ret;
    struct hostent *servhost;
    struct sockaddr_in server;
    struct servent *service;
    linux_socket_context_t* ctx;
    
    socket_context->app_context = malloc(sizeof(linux_socket_context_t));

    ctx = (linux_socket_context_t*)socket_context->app_context;
    memset(ctx, 0x00, sizeof(linux_socket_context_t));

    servhost = gethostbyname(host);
    if (servhost == NULL) {
        printf("failed to get host.\n");
        return KII_SOCKETC_FAIL;
    }
    memset(&server, 0x00, sizeof(server));
    server.sin_family = AF_INET;
    memcpy(&(server.sin_addr), servhost->h_addr, servhost->h_length);

    server.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("failed to init socket.\n");
        return KII_SOCKETC_FAIL;
    }

    if (connect(sock, (struct sockaddr*) &server, sizeof(server)) == -1 ){
        printf("failed to connect socket.\n");
        return KII_SOCKETC_FAIL;
    }
    ctx->sock = sock;
    return KII_SOCKETC_OK;
}

kii_socket_code_t
    send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length)
{
    linux_socket_context_t* ctx;
    int ret;
    int sock;

    ctx = (linux_socket_context_t*)socket_context->app_context;
    sock = ctx->sock;
    ret = send(sock, buffer, length, 0);
    if (ret > 0) {
        return KII_SOCKETC_OK;
    } else {
        printf("failed to send\n");
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    recv_cb(kii_socket_context_t* socket_context,
            char* buffer,
            size_t length_to_read,
            size_t* out_actual_length)
{
    linux_socket_context_t* ctx;
    int ret;

    ctx = (linux_socket_context_t*)socket_context->app_context;
    ret = recv(ctx->sock, buffer, length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;
        return KII_SOCKETC_OK;
    } else {
        printf("failed to receive:\n");
        /* TOOD: could be 0 on success? */
        *out_actual_length = 0;
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    close_cb(kii_socket_context_t* socket_context)
{
    linux_socket_context_t* ctx;
    int ret;

    ctx = (linux_socket_context_t*)socket_context->app_context;
    close(ctx->sock);
    free(ctx);
    return KII_SOCKETC_OK;
}

