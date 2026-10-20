/*
 * ebisu_cc3200.h
 *
 *  Created on: 2018/12/21
 *      Author: kiidemo
 */

#ifndef EBISU_CC3200_H_
#define EBISU_CC3200_H_

#include "khc_socket_callback.h"
#include "kii_task_callback.h"

typedef struct socket_context_t {
    int sock;
    unsigned int to_recv;
    unsigned int to_send;
    /* What the last sl_Recv() that failed returned, for
       sock_cb_recv_timed_out(). */
    int recv_error;
} socket_context_t;

typedef struct {
    unsigned int stk_size;
    unsigned int priority;
} task_context_t;

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

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata);

void delay_ms_cb_impl(uint32_t msec, void* userdata);

#endif /* EBISU_CC3200_H_ */
