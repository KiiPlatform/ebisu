#ifndef TASK_IMPL_H
#define TASK_IMPL_H

#include <kii_task_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     void* userdata);

void delay_ms_cb
    (unsigned int msec,
     void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* TASK_IMPL_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
