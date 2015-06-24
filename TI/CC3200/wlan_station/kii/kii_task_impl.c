#include "kii_task_impl.h"

#include <string.h>
#include<stdlib.h>

#include "simplelink.h"
#include "osi.h"
#include "rom_map.h"

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     unsigned char* stk_start,
     unsigned int stk_size,
     unsigned int priority)
{
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
