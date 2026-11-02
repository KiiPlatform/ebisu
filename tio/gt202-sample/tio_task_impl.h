#ifndef _TIO_TASK_IMPL
#define _TIO_TASK_IMPL

#include <ebisu_system.h>
#include <mqx.h>
#include <mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    TIO_MQTT_TASK    = 1001,
    TIO_UPDATER_TASK = 1002
};

void _tio_task(uint_32 temp);

/* The system: tasks are MQX tasks, delays _time_delay(), and the monotonic
   clock _time_get_elapsed(). It has no state, so one serves everything. */
ebisu_system_t* gt202_system(void);

/* A lock that is an MQX mutex. */
typedef struct {
    ebisu_lock_t base;
    MUTEX_STRUCT mutex;
} gt202_lock_t;

/* Initializes the mutex, and makes base a lock of it. What _mutex_init()
   returns. */
uint_32 gt202_lock_init(gt202_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* _TIO_TASK_IMPL */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
