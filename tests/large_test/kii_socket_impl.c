#include "kii_socket_impl.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port)
{
    int sock;
    struct hostent *servhost;
    struct sockaddr_in server;
    
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
    socket_context->socket = sock;
    return KII_SOCKETC_OK;
}

kii_socket_code_t
    send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length)
{
    int ret;
    int sock;

    sock = socket_context->socket;
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
    int ret;

    ret = recv(socket_context->socket, buffer, length_to_read, 0);
    if (ret > 0) {
        *out_actual_length = ret;
        return KII_SOCKETC_OK;
    } else {
        printf("failed to receive:\n");
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    close_cb(kii_socket_context_t* socket_context)
{
    int sock;
    sock = socket_context->socket;

    close(sock);
    socket_context->socket = -1;
    return KII_SOCKETC_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
