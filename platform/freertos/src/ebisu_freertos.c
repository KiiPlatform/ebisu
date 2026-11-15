#include "ebisu_freertos.h"

#include <ebisu_object.h>

#include <limits.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#if configUSE_MUTEXES != 1
#error "ebisu_freertos_lock_t is a FreeRTOS mutex: set configUSE_MUTEXES to 1."
#endif
#if INCLUDE_vTaskDelay != 1
#error "The system's delays are vTaskDelay(): set INCLUDE_vTaskDelay to 1."
#endif
#if configSUPPORT_DYNAMIC_ALLOCATION == 1 && INCLUDE_vTaskDelete != 1
#error "The system's tasks delete themselves when they end: set INCLUDE_vTaskDelete to 1."
#endif

#if configSUPPORT_DYNAMIC_ALLOCATION == 1

/* What a task runs, for task_main(): a FreeRTOS task returns nothing, and is
   deleted rather than return. */
typedef struct task_start_t {
    EBISU_TASK_ENTRY entry;
    void* param;
} task_start_t;

static void task_main(void* arg)
{
    task_start_t start = *(task_start_t*)arg;
    vPortFree(arg);
    (void)start.entry(start.param);
    vTaskDelete(NULL);
}

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* entry_param)
{
    ebisu_freertos_system_t* system = EBISU_CONTAINER_OF(self, ebisu_freertos_system_t, base);
    task_start_t* start = (task_start_t*)pvPortMalloc(sizeof(*start));
    if (start == NULL) {
        return EBISU_TASKC_FAIL;
    }
    start->entry = entry;
    start->param = entry_param;
    if (xTaskCreate(task_main, name, system->_stack_depth, start, system->_priority, NULL) !=
            pdPASS) {
        vPortFree(start);
        return EBISU_TASKC_FAIL;
    }
    return EBISU_TASKC_OK;
}

#endif /* configSUPPORT_DYNAMIC_ALLOCATION == 1 */

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    uint64_t ticks;
    (void)self;
    if (msec == 0) {
        /* A yield. */
        vTaskDelay(0);
        return;
    }
    /* Rounded up, and a tick more: the tick under way counts as a whole one,
       so that n ticks can be a little less than n of their periods. */
    ticks = ((uint64_t)msec * configTICK_RATE_HZ + 999u) / 1000u + 1u;
    /* More than a delay takes, with ticks of 16 bits, or 32 at a rate above
       1 kHz, in turns. */
    while (ticks > 0) {
        TickType_t turn = ticks > portMAX_DELAY ? portMAX_DELAY : (TickType_t)ticks;
        vTaskDelay(turn);
        ticks -= turn;
    }
}

static uint64_t monotonic_ms(ebisu_system_t* self)
{
    TimeOut_t now;
    uint64_t ticks;
    (void)self;
    /* The tick count and the times it has overflowed, read together, as
       FreeRTOS reads them for its own timeouts. The count of overflows is a
       BaseType_t: on a port whose BaseType_t is 16 bits, with ticks of 16 too,
       the clock wraps after 2^32 ticks. */
    vTaskSetTimeOutState(&now);
#if configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_64_BITS
    ticks = (uint64_t)now.xTimeOnEntering;
#else
    ticks = ((uint64_t)(UBaseType_t)now.xOverflowCount << (sizeof(TickType_t) * CHAR_BIT)) +
        now.xTimeOnEntering;
#endif
    return ticks * 1000u / configTICK_RATE_HZ;
}

static const ebisu_system_ops_t system_ops = {
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    task_create,
#else
    NULL,
#endif
    delay_ms,
    monotonic_ms
};

void ebisu_freertos_system_init(
        ebisu_freertos_system_t* system,
        configSTACK_DEPTH_TYPE stack_depth,
        UBaseType_t priority)
{
    system->base.ops = &system_ops;
    system->_stack_depth = stack_depth;
    system->_priority = priority;
}

static void lock(ebisu_lock_t* self)
{
    ebisu_freertos_lock_t* lock = EBISU_CONTAINER_OF(self, ebisu_freertos_lock_t, base);
    /* portMAX_DELAY waits for ever only with INCLUDE_vTaskSuspend, and is
       else a timeout like any other, of 65 seconds with ticks of 16 bits. */
    while (xSemaphoreTake(lock->_mutex, portMAX_DELAY) != pdTRUE) {
    }
}

static void unlock(ebisu_lock_t* self)
{
    ebisu_freertos_lock_t* lock = EBISU_CONTAINER_OF(self, ebisu_freertos_lock_t, base);
    (void)xSemaphoreGive(lock->_mutex);
}

static const ebisu_lock_ops_t lock_ops = {
    lock,
    unlock
};

bool ebisu_freertos_lock_init(ebisu_freertos_lock_t* lock)
{
    lock->base.ops = &lock_ops;
#if configSUPPORT_STATIC_ALLOCATION == 1
    lock->_mutex = xSemaphoreCreateMutexStatic(&lock->_storage);
#else
    lock->_mutex = xSemaphoreCreateMutex();
#endif
    return lock->_mutex != NULL;
}

void ebisu_freertos_lock_destroy(ebisu_freertos_lock_t* lock)
{
    vSemaphoreDelete(lock->_mutex);
    lock->_mutex = NULL;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
