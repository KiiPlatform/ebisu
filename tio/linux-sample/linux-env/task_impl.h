#ifndef _KII_TASK_IMPL
#define _KII_TASK_IMPL

#include <ebisu_task.h>

#ifdef __cplusplus
extern "C" {
#endif

ebisu_task_code_t task_create_cb
    (const char* name,
     EBISU_TASK_ENTRY entry,
     void* param,
     void* userdata);

void delay_ms_cb
    (uint32_t msec,
     void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
