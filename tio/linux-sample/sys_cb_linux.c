#include "sys_cb_impl.h"
#include "linux-env/task_impl.h"

#include <stdio.h>
#include <stdarg.h>

void delay_ms_cb_impl(unsigned int msec, void* userdata)
{
    delay_ms_cb(msec, userdata);
}

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    return task_create_cb(name, entry, param, userdata);
}
