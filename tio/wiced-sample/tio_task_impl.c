#include "tio_task_impl.h"

#include "tio.h"
#include "kii.h"

#include "wiced.h"
#include "wiced_log.h"

typedef struct _task_thread_arg {
    wiced_thread_t thread;
    KII_TASK_ENTRY entry;
    void* param;
    void* userdata;
} task_thread_arg_t;

void task_thread_function( wiced_thread_arg_t arg ) {
    task_thread_arg_t* task_arg = (task_thread_arg_t*)arg;

    task_arg->entry(task_arg->param);
}

static task_thread_arg_t tio_mqtt_task;
static task_thread_arg_t tio_updater_task;

kii_task_code_t cb_task_create
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     void* userdata)
{
    unsigned int stk_size = WICED_DEFAULT_APPLICATION_STACK_SIZE;
    unsigned int priority = RTOS_DEFAULT_THREAD_PRIORITY;
    task_thread_arg_t *task_arg = NULL;

    if (strcmp(name, TIO_TASK_NAME_UPDATE_STATE) == 0) {
        task_arg = &tio_updater_task;
    } else if (strcmp(name, KII_TASK_NAME_MQTT) == 0) {
        task_arg = &tio_mqtt_task;
    } else {
        wiced_log_printf("unknown task name: %s\n", name);
        return KII_TASKC_FAIL;
    }

    task_arg->entry = entry;
    task_arg->param = param;
    task_arg->userdata = userdata;
    if (wiced_rtos_create_thread(&(task_arg->thread), priority, name, task_thread_function, stk_size, task_arg) != WICED_SUCCESS) {
        wiced_log_printf("create thread [%s] failed.\n", name);
        return KII_TASKC_FAIL;
    } else {
        return KII_TASKC_OK;
    }
}

void cb_delay_ms(unsigned int msec, void* userdata)
{
    wiced_rtos_delay_milliseconds(msec);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
