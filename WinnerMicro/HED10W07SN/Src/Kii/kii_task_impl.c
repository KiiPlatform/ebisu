#include "kii_task_impl.h"
#include "wm_include.h"

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     unsigned char* stk_start,
     unsigned int stk_size,
     unsigned int priority)
{
    int ret;

	ret = tls_os_task_create(NULL,name, entry, param, stk_start, stk_size, priority, 0);

    if(ret == TLS_OS_SUCCESS)
    {
        return KII_TASKC_OK;
    }
    else
    {
        return KII_TASKC_FAIL;
    }
}

void delay_ms_cb(unsigned int msec)
{
	tls_os_time_delay(msec*HZ/1000);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
