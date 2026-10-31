#include "ebisu_cc3200.h"

#include <stdio.h>
#include <stdarg.h>

#include "simplelink.h"
#include "osi.h"
#include "FreeRTOS.h"
#include "task.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

khc_sock_code_t sock_cb_connect(
        void* socket_context,
        const char* host,
        uint16_t port)
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
    uint16_t port_ = port;
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
        ctx->recv_error = ret;
        return KHC_SOCK_FAIL;
    }
}

/* sock_cb_recv() returns KHC_SOCK_FAIL both when the receive timeout set with
   SL_SO_RCVTIMEO in sock_cb_connect() runs out, for which sl_Recv() returns
   SL_EAGAIN, and when the connection fails. */
kii_bool_t sock_cb_recv_timed_out(void* socket_context)
{
    socket_context_t* ctx = (socket_context_t*) socket_context;
    return ctx->recv_error == SL_EAGAIN ? KII_TRUE : KII_FALSE;
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

void delay_ms_cb_impl(uint32_t msec, void* userdata)
{
    osi_Sleep(msec);
}

void lock_cb_impl(void* userdata)
{
    osi_LockObjLock((OsiLockObj_t*)userdata, OSI_WAIT_FOREVER);
}

void unlock_cb_impl(void* userdata)
{
    osi_LockObjUnlock((OsiLockObj_t*)userdata);
}

/* xTaskGetTickCount() counts in 32 bits, which at the SDK's configTICK_RATE_HZ
 * of 1000 wrap after about 49.7 days. ebisu calls this only with the session
 * lock held, so the wraps can be counted here without a lock of their own. */
uint64_t monotonic_ms_cb_impl(void* userdata)
{
    static uint32_t last = 0;
    static uint64_t wraps = 0;
    uint32_t now = (uint32_t)xTaskGetTickCount();
    if (now < last) {
        wraps += (uint64_t)1 << 32;
    }
    last = now;
    return (wraps + now) * 1000 / configTICK_RATE_HZ;
}
