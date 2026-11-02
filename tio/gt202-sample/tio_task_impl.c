#include <mqx.h>
#include <mutex.h>
#include "main.h"
#include "tio_task_impl.h"

typedef struct _data {
    const char* name;
    EBISU_TASK_ENTRY entry;
    void* param;
} _data_t;

void _tio_task(uint_32 temp)
{
    _data_t* data = (_data_t*)temp;
    data->entry(data->param);
    free(data);
}

static ebisu_task_code_t task_create
    (ebisu_system_t* self,
     const char* name,
     EBISU_TASK_ENTRY entry,
     void* param)
{
    _data_t* data = NULL;
    uint_32 task = TIO_MQTT_TASK;

    data = (_data_t*)malloc(sizeof(_data_t));
    data->name = name;
    data->entry = entry;
    data->param = param;

    if (ATH_STRCMP(name, "task_update_state") == 0) {
      task = TIO_UPDATER_TASK;
    }

    _task_create(0, task, (uint_32) data);

    return EBISU_TASKC_OK;
}

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    _time_delay(msec);
}

static void lock(ebisu_lock_t* self)
{
    _mutex_lock(&EBISU_CONTAINER_OF(self, gt202_lock_t, base)->mutex);
}

static void unlock(ebisu_lock_t* self)
{
    _mutex_unlock(&EBISU_CONTAINER_OF(self, gt202_lock_t, base)->mutex);
}

static const ebisu_lock_ops_t gt202_lock_ops = {
    lock,
    unlock
};

uint_32 gt202_lock_init(gt202_lock_t* lock)
{
    lock->base.ops = &gt202_lock_ops;
    return _mutex_init(&lock->mutex, NULL);
}

/* _time_get_elapsed() is the time since MQX started, which _time_set() leaves
   alone. Its seconds are 32 bits wide, so it does not wrap for 136 years. */
static uint64_t monotonic_ms(ebisu_system_t* self)
{
    TIME_STRUCT elapsed;
    _time_get_elapsed(&elapsed);
    return (uint64_t)elapsed.SECONDS * 1000 + elapsed.MILLISECONDS;
}
static const ebisu_system_ops_t gt202_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

static ebisu_system_t system_ = { &gt202_system_ops };

ebisu_system_t* gt202_system(void)
{
    return &system_;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
