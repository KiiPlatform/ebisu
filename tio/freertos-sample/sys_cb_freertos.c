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
    EBISU_TASK_ENTRY entry;
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

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    task_start_t* start = malloc(sizeof(task_start_t));
    if (start == NULL) {
        return EBISU_TASKC_FAIL;
    }
    start->entry = entry;
    start->param = param;
    if (xTaskCreate(task_main, name, TASK_STACK_WORDS, start, TASK_PRIORITY,
                NULL) != pdPASS) {
        free(start);
        return EBISU_TASKC_FAIL;
    }
    return EBISU_TASKC_OK;
}

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    TickType_t ticks = pdMS_TO_TICKS(msec);
    /* At least a tick, so that a short delay still lets other tasks run. */
    vTaskDelay(ticks > 0 ? ticks : 1);
}

static uint64_t monotonic_ms(ebisu_system_t* self)
{
    /* The tick count wraps around every 49 days at 1 kHz. FreeRTOS counts how
       many times, for its timeouts, and gives both together. */
    TimeOut_t now;
    vTaskSetTimeOutState(&now);
    uint64_t ticks = ((uint64_t)(uint32_t)now.xOverflowCount << 32) | now.xTimeOnEntering;
    return ticks * 1000 / configTICK_RATE_HZ;
}

static const ebisu_system_ops_t freertos_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

static ebisu_system_t system_ = { &freertos_system_ops };

ebisu_system_t* freertos_system(void)
{
    return &system_;
}

static void lock(ebisu_lock_t* self)
{
    xSemaphoreTake(EBISU_CONTAINER_OF(self, freertos_lock_t, base)->mutex, portMAX_DELAY);
}

static void unlock(ebisu_lock_t* self)
{
    xSemaphoreGive(EBISU_CONTAINER_OF(self, freertos_lock_t, base)->mutex);
}

static const ebisu_lock_ops_t freertos_lock_ops = {
    lock,
    unlock
};

void freertos_lock_init(freertos_lock_t* lock)
{
    lock->base.ops = &freertos_lock_ops;
    lock->mutex = xSemaphoreCreateMutex();
}
