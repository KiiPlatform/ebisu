#ifndef SYS_CB_ESP32_H
#define SYS_CB_ESP32_H

#include <ebisu_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The system: tasks are FreeRTOS tasks of their own, delays vTaskDelay(), and
   the monotonic clock is milliseconds since boot, from esp_timer, which keeps
   counting through light sleep. Deep sleep restarts the device. It has no
   state, so one serves everything. */
ebisu_system_t* esp32_system(void);

/* A lock that is a FreeRTOS mutex. */
typedef struct {
    ebisu_lock_t base;
    SemaphoreHandle_t mutex;
} esp32_lock_t;

/* Creates the mutex, and makes base a lock of it. */
void esp32_lock_init(esp32_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_ESP32_H */
