#ifndef _TIO_TASK_IMPL
#define _TIO_TASK_IMPL

#include "kii_task_callback.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    TIO_MQTT_TASK    = 1001,
    TIO_UPDATER_TASK = 1002
};

void _tio_task(uint_32 temp);

kii_task_code_t cb_task_create(
    const char* name,
    KII_TASK_ENTRY entry,
    void* param,
    void* userdata);

void cb_delay_ms(
    unsigned int msec,
    void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _TIO_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
