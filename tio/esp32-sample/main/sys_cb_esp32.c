#include "sys_cb_esp32.h"

#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>

#include "sdkconfig.h"

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
    if (xTaskCreate(task_main, name, CONFIG_EBISU_SAMPLE_TASK_STACK_SIZE, start,
                tskIDLE_PRIORITY + 5, NULL) != pdPASS) {
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
    return (uint64_t)esp_timer_get_time() / 1000;
}

static const ebisu_system_ops_t esp32_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

static ebisu_system_t system_ = { &esp32_system_ops };

ebisu_system_t* esp32_system(void)
{
    return &system_;
}

static void lock(ebisu_lock_t* self)
{
    xSemaphoreTake(EBISU_CONTAINER_OF(self, esp32_lock_t, base)->mutex, portMAX_DELAY);
}

static void unlock(ebisu_lock_t* self)
{
    xSemaphoreGive(EBISU_CONTAINER_OF(self, esp32_lock_t, base)->mutex);
}

static const ebisu_lock_ops_t esp32_lock_ops = {
    lock,
    unlock
};

void esp32_lock_init(esp32_lock_t* lock)
{
    lock->base.ops = &esp32_lock_ops;
    lock->mutex = xSemaphoreCreateMutex();
}
