#ifndef SYS_CB_FREERTOS_H
#define SYS_CB_FREERTOS_H

#include <ebisu_system.h>
#include <FreeRTOS.h>
#include <semphr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The system: tasks are FreeRTOS tasks of their own, delays vTaskDelay(), and
   the monotonic clock is milliseconds since the scheduler started, from
   FreeRTOS's tick count, which tickless idle keeps counting. A sleep that stops
   the tick's timer stops it too. It has no state, so one serves everything. */
ebisu_system_t* freertos_system(void);

/* A lock that is a FreeRTOS mutex. */
typedef struct {
    ebisu_lock_t base;
    SemaphoreHandle_t mutex;
} freertos_lock_t;

/* Creates the mutex, and makes base a lock of it. */
void freertos_lock_init(freertos_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_FREERTOS_H */
