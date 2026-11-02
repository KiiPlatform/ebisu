#include "ebisu_cc3200.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "simplelink.h"
#include "osi.h"
#include "FreeRTOS.h"
#include "task.h"
#ifndef NOTERM
#include "uart_if.h"
#endif

ebisu_sock_code_t sock_cb_connect(
        ebisu_socket_t* self,
        const char* host,
        uint16_t port)
{
    unsigned long destinationIP;
    SlSockAddrIn_t  addr;
    int sock;

    if(sl_NetAppDnsGetHostByName((signed char*)host, strlen(host),
                &destinationIP, SL_AF_INET) < 0){
        return EBISU_SOCK_FAIL;
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
        return EBISU_SOCK_FAIL;
    }
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    SlTimeval_t torecv;
    torecv.tv_sec = ctx->to_recv;
    torecv.tv_usec = 0;
    if (sl_SetSockOpt(sock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, &torecv,
                    sizeof(torecv)) != 0) {
        return EBISU_SOCK_FAIL;
    }

    if (sl_Connect(sock, ( SlSockAddr_t *)&addr, sizeof(struct SlSockAddrIn_t)) < 0) {
        sl_Close(sock);
        return EBISU_SOCK_FAIL;
    }

    ctx->sock = sock;
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t sock_cb_send(
        ebisu_socket_t* self,
        const char* buffer,
        size_t length,
        size_t* out_length)
{
    int ret;
    int sock;

    Report("%.*s", length, buffer);
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);

    sock = ctx->sock;
    ret = sl_Send(sock, buffer, length, 0);
    *out_length = ret;
    if (ret > 0) {
        return EBISU_SOCK_OK;
    } else {
        return EBISU_SOCK_FAIL;
    }
}

ebisu_sock_code_t sock_cb_recv(
        ebisu_socket_t* self,
        char* buffer,
        size_t length_to_read,
        size_t* out_actual_length)
{
    int ret;
    int sock;

    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);

    sock = ctx->sock;
    ret = sl_Recv(sock, buffer, length_to_read, 0);
    if (ret >= 0) {
        *out_actual_length = ret;
        Report("%.*s", ret, buffer);
        return EBISU_SOCK_OK;
    } else {
        ctx->recv_error = ret;
        return EBISU_SOCK_FAIL;
    }
}

/* sock_cb_recv() returns EBISU_SOCK_FAIL both when the receive timeout set with
   SL_SO_RCVTIMEO in sock_cb_connect() runs out, for which sl_Recv() returns
   SL_EAGAIN, and when the connection fails. */
bool sock_cb_recv_timed_out(ebisu_socket_t* self)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    return ctx->recv_error == SL_EAGAIN;
}

ebisu_sock_code_t sock_cb_close(ebisu_socket_t* self)
{
    int sock;
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    sock = ctx->sock;

    sl_Close(sock);
    ctx->sock = -1;
    return EBISU_SOCK_OK;
}


static const ebisu_socket_ops_t sock_cb_ops = {
    sock_cb_connect,
    sock_cb_send,
    sock_cb_recv,
    sock_cb_close,
    sock_cb_recv_timed_out
};

void sock_cb_init_context(
        socket_context_t* ctx,
        unsigned int to_recv,
        unsigned int to_send)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->base.ops = &sock_cb_ops;
    ctx->sock = -1;
    ctx->to_recv = to_recv;
    ctx->to_send = to_send;
}

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    cc3200_system_t* ctx = EBISU_CONTAINER_OF(self, cc3200_system_t, base);
    if (osi_TaskCreate(entry, (const signed char*) name, ctx->stk_size, param, ctx->priority, NULL) < 0) {
        return EBISU_TASKC_FAIL;
    } else {
        return EBISU_TASKC_OK;
    }
}

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    osi_Sleep(msec);
}

static void lock(ebisu_lock_t* self)
{
    osi_LockObjLock(&EBISU_CONTAINER_OF(self, cc3200_lock_t, base)->lock, OSI_WAIT_FOREVER);
}

static void unlock(ebisu_lock_t* self)
{
    osi_LockObjUnlock(&EBISU_CONTAINER_OF(self, cc3200_lock_t, base)->lock);
}

static const ebisu_lock_ops_t cc3200_lock_ops = {
    lock,
    unlock
};

OsiReturnVal_e cc3200_lock_init(cc3200_lock_t* lock)
{
    lock->base.ops = &cc3200_lock_ops;
    return osi_LockObjCreate(&lock->lock);
}

/* xTaskGetTickCount() counts in 32 bits, which at the SDK's configTICK_RATE_HZ
 * of 1000 wrap after about 49.7 days. ebisu calls this only with the session
 * lock held, so the wraps can be counted here without a lock of their own. */
static uint64_t monotonic_ms(ebisu_system_t* self)
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

static const ebisu_system_ops_t cc3200_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

void cc3200_system_init(
        cc3200_system_t* system,
        unsigned int stk_size,
        unsigned int priority)
{
    system->base.ops = &cc3200_system_ops;
    system->stk_size = stk_size;
    system->priority = priority;
}
