#ifndef _TIO_TASK_IMPL
#define _TIO_TASK_IMPL

#include "kii_task_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_task_code_t cb_task_create(
    const char* name,
    KII_TASK_ENTRY entry,
    void* param,
    void* userdata);

void cb_delay_ms(
    uint32_t msec,
    void* userdata);

/* Session lock callbacks: userdata is an initialized wiced_mutex_t. */
void cb_lock(void* userdata);

void cb_unlock(void* userdata);

uint64_t cb_monotonic_ms(void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _TIO_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
