#include "sys_cb_freertos.h"

#include <stdlib.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

/* In words. Each task makes TLS connections, whose handshake takes the most of
   it. */
#define TASK_STACK_WORDS 3072
/* Below FreeRTOS-Plus-TCP's tasks. */
#define TASK_PRIORITY (tskIDLE_PRIORITY + 2)

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
    if (xTaskCreate(task_main, name, TASK_STACK_WORDS, start, TASK_PRIORITY,
                NULL) != pdPASS) {
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
    /* The tick count wraps around every 49 days at 1 kHz. FreeRTOS counts how
       many times, for its timeouts, and gives both together. */
    TimeOut_t now;
    vTaskSetTimeOutState(&now);
    uint64_t ticks = ((uint64_t)(uint32_t)now.xOverflowCount << 32) | now.xTimeOnEntering;
    return ticks * 1000 / configTICK_RATE_HZ;
}
