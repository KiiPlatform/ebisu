#ifndef SYS_CB_ESP32_H
#define SYS_CB_ESP32_H

#include <kii_task_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runs entry(param) in a FreeRTOS task of its own. */
kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata);

void delay_ms_cb_impl(uint32_t msec, void* userdata);

/* userdata is a SemaphoreHandle_t, a mutex. */
void lock_cb_impl(void* userdata);
void unlock_cb_impl(void* userdata);

/* Milliseconds since boot, from esp_timer, which keeps counting through light
   sleep. Deep sleep restarts the device. */
uint64_t monotonic_ms_cb_impl(void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_ESP32_H */
