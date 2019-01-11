#include "ebisu_cc3200.h"

#include <stdio.h>
#include <stdarg.h>

#include "simplelink.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

khc_sock_code_t sock_cb_connect(
        void* socket_context,
        const char* host,
        unsigned int port)
{
    unsigned long destinationIP;
    SlSockAddrIn_t  addr;
    int sock;

    if(sl_NetAppDnsGetHostByName((signed char*)host, strlen(host),
                &destinationIP, SL_AF_INET) < 0){
        return KHC_SOCK_FAIL;
    }
    memset(&addr, 0x00, sizeof(struct SlSockAddrIn_t));
    addr.sin_family = SL_AF_INET;
    unsigned int port_ = port;
    if (port == 443) {
        port_ = 80;
    }
    addr.sin_port = sl_Htons(port_);
    addr.sin_addr.s_addr = sl_Htonl(destinationIP);
    sock = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if (sock < 0) {
        return KHC_SOCK_FAIL;
    }
    socket_context_t* ctx = (socket_context_t*) socket_context;
    SlTimeval_t torecv;
    torecv.tv_sec = ctx->to_recv;
    torecv.tv_usec = 0;
    if (sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, &torecv,
                    sizeof(torecv)) != 0) {
        return KHC_SOCK_FAIL;
    }

    if (sl_Connect(sock, ( SlSockAddr_t *)&addr, sizeof(struct SlSockAddrIn_t)) < 0) {
        sl_Close(sock);
        return KHC_SOCK_FAIL;
    }

    ctx->sock = sock;
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_cb_send(
        void* socket_context,
        const char* buffer,
        size_t length,
        size_t* out_length)
{
    int ret;
    int sock;

    Report("%.*s", length, buffer);
    socket_context_t* ctx = (socket_context_t*) socket_context;

    sock = ctx->sock;
    ret = sl_Send(sock, buffer, length, 0);
    *out_length = ret;
    if (ret > 0) {
        return KHC_SOCK_OK;
    } else {
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_recv(
        void* socket_context,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    int ret;
    int sock;

    socket_context_t* ctx = (socket_context_t*) socket_context;

    sock = ctx->sock;
    ret = sl_Recv(sock, buffer, length_to_read, 0);
    if (ret >= 0) {
        *out_actual_length = ret;
        Report("%.*s", ret, buffer);
        return KHC_SOCK_OK;
    } else {
        return KHC_SOCK_FAIL;
    }
}

khc_sock_code_t sock_cb_close(void* socket_context)
{
    int sock;
    socket_context_t* ctx = (socket_context_t*) socket_context;
    sock = ctx->sock;

    sl_Close(sock);
    ctx->sock = -1;
    return KHC_SOCK_OK;
}


kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    task_context_t* ctx = (task_context_t*) userdata;
    if (osi_TaskCreate(entry, (const signed char*) name, ctx->stk_size, param, ctx->priority, NULL) < 0) {
        return KII_TASKC_FAIL;
    } else {
        return KII_TASKC_OK;
    }
}

void delay_ms_cb_impl(unsigned int msec, void* userdata)
{
    osi_Sleep(msec);
}
