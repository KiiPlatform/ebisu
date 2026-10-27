#include "sys_cb_esp32.h"

#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>

#include "sdkconfig.h"

typedef struct {
    KII_TASK_ENTRY entry;
    void* param;
} task_start_t;

/* A FreeRTOS task returns nothing and must not return: it deletes itself. */
static void task_main(void* arg)
{
    task_start_t start = *(task_start_t*)arg;
    free(arg);
    start.entry(start.param);
    vTaskDelete(NULL);
}

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    task_start_t* start = malloc(sizeof(task_start_t));
    if (start == NULL) {
        return KII_TASKC_FAIL;
    }
    start->entry = entry;
    start->param = param;
    if (xTaskCreate(task_main, name, CONFIG_EBISU_SAMPLE_TASK_STACK_SIZE, start,
                tskIDLE_PRIORITY + 5, NULL) != pdPASS) {
        free(start);
        return KII_TASKC_FAIL;
    }
    return KII_TASKC_OK;
}

void delay_ms_cb_impl(uint32_t msec, void* userdata)
{
    TickType_t ticks = pdMS_TO_TICKS(msec);
    /* At least a tick, so that a short delay still lets other tasks run. */
    vTaskDelay(ticks > 0 ? ticks : 1);
}

void lock_cb_impl(void* userdata)
{
    xSemaphoreTake((SemaphoreHandle_t)userdata, portMAX_DELAY);
}

void unlock_cb_impl(void* userdata)
{
    xSemaphoreGive((SemaphoreHandle_t)userdata);
}

uint64_t monotonic_ms_cb_impl(void* userdata)
{
    return (uint64_t)esp_timer_get_time() / 1000;
}
