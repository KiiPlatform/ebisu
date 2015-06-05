
#include "kii_task_impl.h"
#include <LTask.h>

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     unsigned char* stk_start,
     unsigned int stk_size,
     unsigned int priority)
{
    return KII_TASKC_OK;
}

void delay_ms_cb(unsigned int msec)
{
    delay((uint32_t)msec);
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
