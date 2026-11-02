#ifndef _KII_TASK_IMPL
#define _KII_TASK_IMPL

#include <ebisu_system.h>

#ifdef __cplusplus
extern "C" {
#endif

ebisu_task_code_t task_create_cb
    (ebisu_system_t* self,
     const char* name,
     EBISU_TASK_ENTRY entry,
     void* param);

void delay_ms_cb
    (ebisu_system_t* self,
     uint32_t msec);

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
