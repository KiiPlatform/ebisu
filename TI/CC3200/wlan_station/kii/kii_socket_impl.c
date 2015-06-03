#include "kii_socket_impl.h"
#include <string.h>
#include <stdio.h>

#include "simplelink.h"

kii_socket_code_t
    connect_cb(kii_socket_context_t* socket_context, const char* host,
            unsigned int port)
{
    unsigned long destinationIP;
    SlSockAddrIn_t  addr;
    int sock;

	if(sl_NetAppDnsGetHostByName((signed char*)host, strlen(host),
	                             &destinationIP, SL_AF_INET) < 0){
        return KII_SOCKETC_FAIL;
	}
    memset(&addr, 0x00, sizeof(struct SlSockAddrIn_t));
    addr.sin_family = SL_AF_INET;
    addr.sin_port = sl_Htons(port);
    addr.sin_addr.s_addr = sl_Htonl(destinationIP);
    sock = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if (sock < 0) {
        return KII_SOCKETC_FAIL;
    }
    if (sl_Connect(sock, ( SlSockAddr_t *)&addr, sizeof(struct SlSockAddrIn_t)) < 0) {
        sl_Close(sock);
		return KII_SOCKETC_FAIL;
    }
    socket_context->sock = sock;
    return KII_SOCKETC_OK;
}

kii_socket_code_t
    send_cb(kii_socket_context_t* socket_context,
            const char* buffer,
            size_t length)
{
    int ret;
    int sock;

    sock = socket_context->sock;
    ret = sl_Send(sock, buffer, length, 0);
    if (ret > 0) {
        return KII_SOCKETC_OK;
    } else {
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
    int sock;

    sock = socket_context->sock;
    ret = sl_Recv(sock, buffer, length_to_read, 0);
    if (ret > 0) {
     *out_actual_length = ret;
     return KII_SOCKETC_OK;
    } else {
        return KII_SOCKETC_FAIL;
    }
}

kii_socket_code_t
    close_cb(kii_socket_context_t* socket_context)
{
    int sock;
    sock = socket_context->sock;

    sl_Close(sock);
    socket_context->sock = -1;
    return KII_SOCKETC_OK;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
