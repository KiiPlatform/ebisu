/*
 * ebisu_cc3200.h
 *
 *  Created on: 2018/12/21
 *      Author: kiidemo
 */

#ifndef EBISU_CC3200_H_
#define EBISU_CC3200_H_

#include <ebisu_socket.h>
#include <ebisu_system.h>
#include "osi.h"

typedef struct socket_context_t {
    /* What tio is given. */
    ebisu_socket_t base;
    int sock;
    unsigned int to_recv;
    unsigned int to_send;
    /* What the last sl_Recv() that failed returned, for
       sock_cb_recv_timed_out(). */
    int recv_error;
} socket_context_t;

/* The system: tasks are OSI tasks of stk_size bytes of stack, at priority,
 * delays osi_Sleep(), and the monotonic clock FreeRTOS's tick count, in
 * milliseconds. */
typedef struct {
    ebisu_system_t base;
    unsigned int stk_size;
    unsigned int priority;
} cc3200_system_t;

/* A lock that is an OsiLockObj_t. */
typedef struct {
    ebisu_lock_t base;
    OsiLockObj_t lock;
} cc3200_lock_t;

/* Clears the context, with the timeouts of the socket, in seconds, and makes
 * its base a socket of the functions below. */
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

void cc3200_system_init(
        cc3200_system_t* system,
        unsigned int stk_size,
        unsigned int priority);

/* Creates the lock object, and makes base a lock of it. What
 * osi_LockObjCreate() returns. */
OsiReturnVal_e cc3200_lock_init(cc3200_lock_t* lock);

#endif /* EBISU_CC3200_H_ */
