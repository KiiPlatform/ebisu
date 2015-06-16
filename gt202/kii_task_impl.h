#ifndef _KII_TASK_IMPL
#define _KII_TASK_IMPL

#include "kii_task_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    KII_TASK = 1000
};

void kii_task(uint_32 temp);

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     unsigned char* stk_start,
     unsigned int stk_size,
     unsigned int priority);

void delay_ms_cb
    (unsigned int msec);

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
