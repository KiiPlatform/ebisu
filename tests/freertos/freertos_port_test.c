/* The FreeRTOS port's tests, a firmware for the MPS2 AN385 board QEMU
   emulates. Each check that fails prints a line on the console, and the
   program ends with 1 if any did, which QEMU exits with. */
#include <ebisu_freertos.h>
#include <ebisu_server_clock.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "board.h"

/* Above the runner's, so that a task the system makes runs as it is made. */
#define SYSTEM_PRIORITY (tskIDLE_PRIORITY + 2)
#define RUNNER_PRIORITY (tskIDLE_PRIORITY + 1)
/* Well above configMINIMAL_STACK_SIZE, for a task to tell it has it. */
#define SYSTEM_STACK 1024
#define HELPER_STACK 512
#define RUNNER_STACK 1024

static ebisu_freertos_system_t test_system;
static int checks;
static int failures;

static void print(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void print(const char* format, ...)
{
    char line[192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length < 0) {
        length = 0;
    } else if ((size_t)length >= sizeof(line)) {
        length = sizeof(line) - 1;
    }
    board_write(line, (size_t)length);
    board_write("\r\n", 2);
}

static void check(bool ok, const char* what, int line)
{
    ++checks;
    if (!ok) {
        ++failures;
        print("FAILED at line %d: %s", line, what);
    }
}

/* A comparison, with the values compared in a failure's line. */
static void check_compare(bool ok, const char* what, uint64_t left, uint64_t right, int line)
{
    ++checks;
    if (!ok) {
        ++failures;
        print("FAILED at line %d: %s, with %llu and %llu", line, what,
                (unsigned long long)left, (unsigned long long)right);
    }
}

#define CHECK(condition) check((condition), #condition, __LINE__)
#define CHECK_LE(left, right) \
    check_compare((uint64_t)(left) <= (uint64_t)(right), #left " <= " #right, (left), (right), \
            __LINE__)
#define CHECK_EQ(left, right) \
    check_compare((uint64_t)(left) == (uint64_t)(right), #left " == " #right, (left), (right), \
            __LINE__)

static uint64_t monotonic(void)
{
    return test_system.base.ops->monotonic_ms(&test_system.base);
}

static void delay(uint32_t msec)
{
    test_system.base.ops->delay_ms(&test_system.base, msec);
}

/* Tasks of the tests' own, which delete themselves when they're done. */
#if configSUPPORT_STATIC_ALLOCATION == 1
#define HELPERS 8
static StaticTask_t helper_tasks[HELPERS];
static StackType_t helper_stacks[HELPERS][HELPER_STACK];
static int helpers_made;
#endif

static void start_helper(TaskFunction_t code, const char* name, UBaseType_t priority)
{
#if configSUPPORT_STATIC_ALLOCATION == 1
    if (helpers_made == HELPERS) {
        board_fatal("more helper tasks than %d", HELPERS);
    }
    (void)xTaskCreateStatic(code, name, HELPER_STACK, NULL, priority,
            helper_stacks[helpers_made], &helper_tasks[helpers_made]);
    ++helpers_made;
#else
    if (xTaskCreate(code, name, HELPER_STACK, NULL, priority, NULL) != pdPASS) {
        board_fatal("failed to make the helper task %s", name);
    }
#endif
}

/* Waits up to msec for the flag. */
static bool wait_for(volatile bool* flag, uint32_t msec)
{
    TickType_t start = xTaskGetTickCount();
    while (!*flag && (TickType_t)(xTaskGetTickCount() - start) < pdMS_TO_TICKS(msec) + 1) {
        vTaskDelay(1);
    }
    return *flag;
}

#if tskKERNEL_VERSION_MAJOR > 10 || (tskKERNEL_VERSION_MAJOR == 10 && tskKERNEL_VERSION_MINOR >= 3)
#define HAVE_CATCH_UP 1
#else
#define HAVE_CATCH_UP 0
#endif

/* Moves the tick count on, as if that time had passed, in turns of 1000
   ticks, which the tasks it wakes run between: a task woken in a turn runs up
   to 1000 ticks late. */
static void catch_up(uint32_t ticks)
{
    while (ticks > 0) {
        TickType_t turn = ticks > 1000 ? 1000 : (TickType_t)ticks;
#if HAVE_CATCH_UP
        (void)xTaskCatchUpTicks(turn);
#else
        /* Before V10.3, as xTaskCatchUpTicks() has done since: a tick counted
           while the scheduler is suspended is pended, and resuming it counts
           the pended ticks. In a critical section, as the tick interrupt
           counts them too. */
        vTaskSuspendAll();
        for (TickType_t i = 0; i < turn; ++i) {
            taskENTER_CRITICAL();
            (void)xTaskIncrementTick();
            taskEXIT_CRITICAL();
        }
        (void)xTaskResumeAll();
#endif
        ticks -= turn;
        vTaskDelay(1);
    }
}

#if configSUPPORT_DYNAMIC_ALLOCATION == 1
/* What the heap has free once the idle task has freed the tasks deleted. */
static size_t heap_free(void)
{
    vTaskDelay(pdMS_TO_TICKS(20) + 1);
    return xPortGetFreeHeapSize();
}
#endif

/* The monotonic clock, while the tick count overflows. */

static volatile bool readers_stop;
static volatile bool reader_went_back;
static volatile int readers_done;

static void reader_task(void* arg)
{
    (void)arg;
    uint64_t last = monotonic();
    while (!readers_stop) {
        uint64_t now = monotonic();
        if (now < last) {
            reader_went_back = true;
        }
        last = now;
    }
    taskENTER_CRITICAL();
    ++readers_done;
    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

static void test_monotonic_overflow(void)
{
    /* Two more reading it all the while, as tasks of the same priority, which
       take turns with this one at each tick. */
    start_helper(reader_task, "reader 1", RUNNER_PRIORITY);
    start_helper(reader_task, "reader 2", RUNNER_PRIORITY);

    /* The clock and the tick count read together, as nothing else is read
       between them in a critical section. */
    taskENTER_CRITICAL();
    const uint64_t first = monotonic();
    TickType_t previous = xTaskGetTickCount();
    taskEXIT_CRITICAL();
    uint64_t ticks = 0;
    uint64_t last = first;
    bool went_back = false;
    int after = -1;
    /* Up to 10 seconds, and on for 50 ticks once it has overflowed. */
    for (int i = 0; i < 10 * TEST_TICK_RATE_HZ && after != 0; ++i) {
        vTaskDelay(1);
        taskENTER_CRITICAL();
        TickType_t now = xTaskGetTickCount();
        uint64_t ms = monotonic();
        taskEXIT_CRITICAL();
        if (now < previous && after < 0) {
            after = 50;
        } else if (after > 0) {
            --after;
        }
        ticks += (TickType_t)(now - previous);
        previous = now;
        if (ms < last) {
            went_back = true;
        }
        last = ms;
    }
    readers_stop = true;
    while (readers_done < 2) {
        vTaskDelay(1);
    }
    CHECK(after == 0);
    CHECK(!went_back);
    CHECK(!reader_went_back);
    /* The ticks counted, in milliseconds, but for rounding. */
    const uint64_t counted = ticks * 1000u / configTICK_RATE_HZ;
    CHECK_LE(counted, last - first + 1);
    CHECK_LE(last - first, counted + 1);
    /* Past what the tick count holds. */
    CHECK_LE(((uint64_t)portMAX_DELAY + 1) * 1000u / configTICK_RATE_HZ, last);
}

/* Tasks. */

#if configSUPPORT_DYNAMIC_ALLOCATION == 1
static struct {
    volatile bool ran;
    void* param;
    TaskHandle_t handle;
    char name[configMAX_TASK_NAME_LEN];
    UBaseType_t priority;
    UBaseType_t free_stack;
} seen;

static void* record(void* param)
{
    seen.param = param;
    seen.handle = xTaskGetCurrentTaskHandle();
    strncpy(seen.name, pcTaskGetName(NULL), sizeof(seen.name) - 1);
    seen.priority = uxTaskPriorityGet(NULL);
    seen.free_stack = uxTaskGetStackHighWaterMark(NULL);
    seen.ran = true;
    return param;
}
#endif

static void test_task_create(void)
{
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    int marker = 0;
    const size_t heap = heap_free();
    CHECK(test_system.base.ops->task_create(&test_system.base, "ebisu task", record, &marker) ==
            EBISU_TASKC_OK);
    CHECK(wait_for(&seen.ran, 1000));
    CHECK(seen.param == &marker);
    CHECK(seen.handle != NULL && seen.handle != xTaskGetCurrentTaskHandle());
    CHECK(strcmp(seen.name, "ebisu task") == 0);
    CHECK_EQ(seen.priority, SYSTEM_PRIORITY);
    /* Of the system's depth, not the least FreeRTOS has. */
    CHECK_LE(seen.free_stack, SYSTEM_STACK);
    CHECK_LE(SYSTEM_STACK / 2, seen.free_stack);
    /* The task, and what was allocated to start it, freed once it ended. */
    CHECK_EQ(heap_free(), heap);
#else
    /* No tasks without the heap. */
    CHECK(test_system.base.ops->task_create == NULL);
#endif
}

static void test_task_create_fails(void)
{
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    /* A stack the heap can't hold. */
    ebisu_freertos_system_t greedy;
    ebisu_freertos_system_init(&greedy,
            (configSTACK_DEPTH_TYPE)(configTOTAL_HEAP_SIZE / sizeof(StackType_t)),
            SYSTEM_PRIORITY);
    seen.ran = false;
    const size_t heap = heap_free();
    CHECK(greedy.base.ops->task_create(&greedy.base, "greedy", record, NULL) ==
            EBISU_TASKC_FAIL);
    CHECK_EQ(heap_free(), heap);
    CHECK(!seen.ran);
#endif
}

/* Delays. */

static void test_delay(void)
{
    static const uint32_t delays[] = { 1, 2, 3, 5, 10, 15, 25, 100, 1000 };
    /* Two ticks late at the most, and some for QEMU. */
    const uint64_t slack = 2 * 1000u / configTICK_RATE_HZ + 20;
    for (size_t i = 0; i < sizeof(delays) / sizeof(delays[0]); ++i) {
        /* The tick under way when it starts may be almost over: n + 1 ticks
           counted are n whole ones at least, of ms. */
        const TickType_t whole =
            (TickType_t)(((uint64_t)delays[i] * configTICK_RATE_HZ + 999u) / 1000u);
        TickType_t ticks_before = xTaskGetTickCount();
        uint64_t before = monotonic();
        delay(delays[i]);
        uint64_t took = monotonic() - before;
        CHECK_LE(whole + 1, (TickType_t)(xTaskGetTickCount() - ticks_before));
        CHECK_LE(delays[i], took);
        CHECK_LE(took, delays[i] + slack);
    }
    /* None, for 0. */
    TickType_t before = xTaskGetTickCount();
    delay(0);
    CHECK_LE((TickType_t)(xTaskGetTickCount() - before), 1);
}

#if TEST_TICK_BITS == 16
/* 75000 ticks at 250 Hz: more than one delay takes, in ticks of 16 bits. */
#define LONG_DELAY_MS 300000u
static volatile bool long_delay_done;

static void long_delay_task(void* arg)
{
    (void)arg;
    delay(LONG_DELAY_MS);
    long_delay_done = true;
    vTaskDelete(NULL);
}
#endif

static void test_long_delay(void)
{
#if TEST_TICK_BITS == 16
    const uint32_t ticks = (uint32_t)((uint64_t)LONG_DELAY_MS * configTICK_RATE_HZ / 1000u);
    CHECK_LE((uint64_t)portMAX_DELAY + 1, ticks);
    const uint64_t before = monotonic();
    start_helper(long_delay_task, "long delay", RUNNER_PRIORITY + 1);
    catch_up(ticks - 5000);
    CHECK(!long_delay_done);
    /* Its second turn starts up to a turn of catch_up() late. */
    catch_up(7000);
    CHECK(wait_for(&long_delay_done, 100));
    /* The clock counted every overflow. */
    CHECK_LE((uint64_t)(ticks + 2000) * 1000u / configTICK_RATE_HZ, monotonic() - before);
#endif
}

/* The lock. */

static ebisu_freertos_lock_t test_lock;
static volatile int counter;
static volatile int counters_done;

static void counter_task(void* arg)
{
    (void)arg;
    for (int i = 0; i < 100; ++i) {
        test_lock.base.ops->lock(&test_lock.base);
        int value = counter;
        /* The other task runs meanwhile, and waits for the lock. */
        vTaskDelay(1);
        counter = value + 1;
        test_lock.base.ops->unlock(&test_lock.base);
    }
    taskENTER_CRITICAL();
    ++counters_done;
    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}

static void test_lock_excludes(void)
{
    start_helper(counter_task, "counter 1", RUNNER_PRIORITY + 1);
    start_helper(counter_task, "counter 2", RUNNER_PRIORITY + 1);
    TickType_t start = xTaskGetTickCount();
    while (counters_done < 2 && (TickType_t)(xTaskGetTickCount() - start) < pdMS_TO_TICKS(10000)) {
        vTaskDelay(1);
    }
    CHECK_EQ(counters_done, 2);
    CHECK_EQ(counter, 200);
}

static volatile bool holding;
static volatile bool release;
static volatile bool waiter_got_it;

static void holder_task(void* arg)
{
    (void)arg;
    test_lock.base.ops->lock(&test_lock.base);
    holding = true;
    while (!release) {
        vTaskDelay(1);
    }
    test_lock.base.ops->unlock(&test_lock.base);
    vTaskDelete(NULL);
}

static void waiter_task(void* arg)
{
    (void)arg;
    test_lock.base.ops->lock(&test_lock.base);
    waiter_got_it = true;
    test_lock.base.ops->unlock(&test_lock.base);
    vTaskDelete(NULL);
}

/* Without INCLUDE_vTaskSuspend, portMAX_DELAY is a timeout, of 65535 ticks
   with ticks of 16 bits: the lock waits on past it. */
static void test_lock_waits(void)
{
    start_helper(holder_task, "holder", RUNNER_PRIORITY + 1);
    CHECK(wait_for(&holding, 1000));
    start_helper(waiter_task, "waiter", RUNNER_PRIORITY + 1);
    vTaskDelay(2);
    catch_up(70000);
    CHECK(!waiter_got_it);
    release = true;
    CHECK(wait_for(&waiter_got_it, 1000));
}

static void test_lock_memory(void)
{
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    ebisu_freertos_lock_t lock;
    const size_t heap = heap_free();
    CHECK(ebisu_freertos_lock_init(&lock));
#if configSUPPORT_STATIC_ALLOCATION == 1
    /* In the lock itself. */
    CHECK_EQ(xPortGetFreeHeapSize(), heap);
#else
    CHECK_LE(xPortGetFreeHeapSize() + sizeof(StaticSemaphore_t), heap);
#endif
    ebisu_freertos_lock_destroy(&lock);
    CHECK_EQ(heap_free(), heap);
#endif
}

/* A lock the system makes, from the heap, and gives back. */
static void test_lock_create(void)
{
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    const size_t heap = heap_free();
    ebisu_lock_t* lock = ebisu_system_lock_create(&test_system.base);
    CHECK(lock != NULL);
    if (lock != NULL) {
        SemaphoreHandle_t mutex = EBISU_CONTAINER_OF(lock, ebisu_freertos_lock_t, base)->_mutex;
        CHECK(heap_free() < heap);
        ebisu_lock_acquire(lock);
        /* Held, and not recursive. */
        CHECK(xSemaphoreTake(mutex, 0) == pdFALSE);
        ebisu_lock_release(lock);
        CHECK(xSemaphoreTake(mutex, 0) == pdTRUE);
        (void)xSemaphoreGive(mutex);
        ebisu_system_lock_destroy(&test_system.base, lock);
    }
    CHECK_EQ(heap_free(), heap);
#else
    CHECK(ebisu_system_lock_create(&test_system.base) == NULL);
#endif
    /* No lock, as in code that runs in one task. */
    ebisu_lock_acquire(NULL);
    ebisu_lock_release(NULL);
}

/* The clock. */

#define FIXED_TIME 1800000000

static bool fixed_now(ebisu_clock_t* self, int64_t* out_now)
{
    (void)self;
    *out_now = FIXED_TIME;
    return true;
}

/* None until the application sets one, then the one it set, whatever the
   clock. */
static void test_clock_set(void)
{
    static const ebisu_clock_ops_t fixed_ops = { fixed_now, NULL, NULL };
    ebisu_clock_t fixed = { &fixed_ops };
    ebisu_freertos_system_t system;
    /* Whatever was in it before. */
    memset(&system, 0xa5, sizeof(system));
    ebisu_freertos_system_init(&system, SYSTEM_STACK, SYSTEM_PRIORITY);
    CHECK(ebisu_system_clock(&system.base) == NULL);
    ebisu_freertos_system_set_clock(&system, &fixed);
    ebisu_clock_t* got = ebisu_system_clock(&system.base);
    CHECK(got == &fixed);
    if (got == &fixed) {
        int64_t now = 0;
        CHECK(ebisu_clock_get_time(got, &now));
        CHECK_EQ(now, FIXED_TIME);
    }
    ebisu_freertos_system_set_clock(&system, NULL);
    CHECK(ebisu_system_clock(&system.base) == NULL);
}

/* A server clock over the system, set on it, counts on from a time given with
   the tick count. */
static void test_clock_counts(void)
{
    ebisu_server_clock_t clock;
    CHECK(ebisu_server_clock_init(&clock, &test_system.base));
    ebisu_freertos_system_set_clock(&test_system, &clock.base);
    ebisu_clock_t* system_clock = ebisu_system_clock(&test_system.base);
    CHECK(system_clock == &clock.base);
    if (system_clock != &clock.base) {
        return;
    }
    int64_t floor = 0;
    CHECK(!ebisu_clock_get_time(system_clock, &floor));
    const int64_t given = floor + 30 * 86400;
    CHECK(ebisu_server_clock_set_time(&clock, given));
    int64_t now = 0;
    CHECK(ebisu_clock_get_time(system_clock, &now));
    CHECK_EQ(now, given);
    catch_up(5 * configTICK_RATE_HZ);
    CHECK(ebisu_clock_get_time(system_clock, &now));
    CHECK_LE(given + 5, now);
    CHECK_LE(now, given + 6);
    ebisu_freertos_system_set_clock(&test_system, NULL);
}

static void runner(void* arg)
{
    (void)arg;
    print("ebisu FreeRTOS port tests: FreeRTOS %s, ticks of %d bits at %d Hz, "
            "static allocation %d, dynamic %d, INCLUDE_vTaskSuspend %d",
            tskKERNEL_VERSION_NUMBER, TEST_TICK_BITS, TEST_TICK_RATE_HZ, TEST_STATIC,
            TEST_DYNAMIC, TEST_SUSPEND);
    ebisu_freertos_system_init(&test_system, SYSTEM_STACK, SYSTEM_PRIORITY);
#if configSUPPORT_DYNAMIC_ALLOCATION == 1
    /* heap_4 sets itself up on its first allocation, and has nothing free
       until then. */
    vPortFree(pvPortMalloc(sizeof(int)));
#endif
    if (!ebisu_freertos_lock_init(&test_lock)) {
        board_fatal("failed to create the lock");
    }
    /* First, before the tick count overflows, 3 seconds after it started. */
    test_monotonic_overflow();
    test_task_create();
    test_task_create_fails();
    test_delay();
    test_long_delay();
    test_lock_excludes();
    test_lock_waits();
    test_lock_memory();
    test_lock_create();
    test_clock_set();
    test_clock_counts();
    print("%d checks, %d failed", checks, failures);
    board_exit(failures == 0 ? 0 : 1);
}

#if configSUPPORT_STATIC_ALLOCATION == 1
static StaticTask_t runner_task;
static StackType_t runner_stack[RUNNER_STACK];
#endif

int main(void)
{
    board_init();
#if configSUPPORT_STATIC_ALLOCATION == 1
    (void)xTaskCreateStatic(runner, "runner", RUNNER_STACK, NULL, RUNNER_PRIORITY, runner_stack,
            &runner_task);
#else
    if (xTaskCreate(runner, "runner", RUNNER_STACK, NULL, RUNNER_PRIORITY, NULL) != pdPASS) {
        board_fatal("failed to make the runner task");
    }
#endif
    vTaskStartScheduler();
    board_fatal("failed to start the scheduler");
}

#if configSUPPORT_STATIC_ALLOCATION == 1 && \
    (tskKERNEL_VERSION_MAJOR < 11 || (tskKERNEL_VERSION_MAJOR == 11 && tskKERNEL_VERSION_MINOR < 1))
/* The idle task's memory, which a kernel before V11.1 asks the application
   for, and later ones give with configKERNEL_PROVIDED_STATIC_MEMORY. */
void vApplicationGetIdleTaskMemory(StaticTask_t** task, StackType_t** stack, uint32_t* depth)
{
    static StaticTask_t idle_task;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];
    *task = &idle_task;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}
#endif

void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    board_fatal("stack overflow in task %s", name);
}

/* The board's vector table has the Ethernet controller's interrupt, which
   FreeRTOS-Plus-TCP's driver handles in the sample. Nothing raises it here. */
void EthernetISR(void)
{
}
