#include "kii_task_impl.h"

#include <string.h>
#include<stdlib.h>

#include "simplelink.h"
#include "osi.h"
#include "rom_map.h"

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param)
{
    int stk_size = 0;
    int priority = 0;

    if (strcmp(name, KII_TASK_NAME_RECV_MSG) == 0) {
        stk_size = 2048;
        priority = 3;
#ifdef KII_PUSH_KEEP_ALIVE_INTERVAL_SECONDS
    } else if (strcmp(name, KII_TASK_NAME_PING_REQ) == 0) {
        stk_size = 1024;
        priority = 4;
#endif
    }

	if (osi_TaskCreate(entry, (const signed char*) name, stk_size, param, priority, NULL) < 0) {
        return KII_TASKC_FAIL;
    } else {
        return KII_TASKC_OK;
    }
	
}

void delay_ms_cb(unsigned int msec)
{
	osi_Sleep(msec);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
