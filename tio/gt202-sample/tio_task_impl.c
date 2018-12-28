#include <mqx.h>
#include "main.h"
#include "tio_task_impl.h"

typedef struct _data {
    const char* name;
    KII_TASK_ENTRY entry;
    void* param;
    void* userdata;
} _data_t;

void _tio_task(uint_32 temp)
{
    _data_t* data = (_data_t*)temp;
    data->entry(data->param);
    free(data);
}

kii_task_code_t cb_task_create
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     void* userdata)
{
    _data_t* data = NULL;
    uint_32 task = TIO_MQTT_TASK;

    data = (_data_t*)malloc(sizeof(_data_t));
    data->name = name;
    data->entry = entry;
    data->param = param;
    data->userdata = userdata;

    if (ATH_STRCMP(name, "task_update_state") == 0) {
      task = TIO_UPDATER_TASK;
    }

    _task_create(0, task, (uint_32) data);

    return KII_TASKC_OK;
}

void cb_delay_ms(unsigned int msec, void* userdata)
{
    _time_delay(msec);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
