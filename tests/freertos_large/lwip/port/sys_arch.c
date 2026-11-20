/* The large tests' lwIP port to FreeRTOS, for lwIP 1.4 and 2.0: its
   semaphores and mailboxes, FreeRTOS's semaphores and queues, its threads,
   FreeRTOS's tasks, and its clock, the tick count. Made from the heap, as
   V8.1.2, the oldest kernel the tests take, has no other way. See lwIP's
   doc/sys_arch.txt. */
#include <lwip/opt.h>
#include <lwip/sys.h>

/* The ticks for a timeout of lwIP's, in milliseconds, rounded up: 0 is none,
   to wait until it comes. */
static TickType_t ticks_for(u32_t ms)
{
    if (ms == 0) {
        return portMAX_DELAY;
    }
    TickType_t ticks = (TickType_t)(((uint64_t)ms * configTICK_RATE_HZ + 999) / 1000);
    return ticks == 0 ? 1 : ticks;
}

static u32_t ms_since(TickType_t start)
{
    return (u32_t)((uint64_t)(xTaskGetTickCount() - start) * 1000 / configTICK_RATE_HZ);
}

void sys_init(void)
{
}

u32_t sys_now(void)
{
    return (u32_t)((uint64_t)xTaskGetTickCount() * 1000 / configTICK_RATE_HZ);
}

/* lwIP's protection of its pools: a critical section, which nests. */
sys_prot_t sys_arch_protect(void)
{
    taskENTER_CRITICAL();
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    taskEXIT_CRITICAL();
}

err_t sys_sem_new(sys_sem_t* sem, u8_t count)
{
    *sem = xSemaphoreCreateCounting(0xffff, count);
    return *sem == NULL ? ERR_MEM : ERR_OK;
}

void sys_sem_signal(sys_sem_t* sem)
{
    (void)xSemaphoreGive(*sem);
}

u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout)
{
    const TickType_t start = xTaskGetTickCount();
    if (timeout == 0) {
        /* portMAX_DELAY is a timeout without INCLUDE_vTaskSuspend: as long as
           it takes. */
        while (xSemaphoreTake(*sem, portMAX_DELAY) != pdTRUE) {
        }
    } else if (xSemaphoreTake(*sem, ticks_for(timeout)) != pdTRUE) {
        return SYS_ARCH_TIMEOUT;
    }
    return ms_since(start);
}

void sys_sem_free(sys_sem_t* sem)
{
    vSemaphoreDelete(*sem);
}

err_t sys_mbox_new(sys_mbox_t* mbox, int size)
{
    *mbox = xQueueCreate(size > 0 ? (UBaseType_t)size : 1, sizeof(void*));
    return *mbox == NULL ? ERR_MEM : ERR_OK;
}

void sys_mbox_post(sys_mbox_t* mbox, void* msg)
{
    while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE) {
    }
}

err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg)
{
    return xQueueSendToBack(*mbox, &msg, 0) == pdTRUE ? ERR_OK : ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout)
{
    void* received = NULL;
    const TickType_t start = xTaskGetTickCount();
    if (timeout == 0) {
        while (xQueueReceive(*mbox, &received, portMAX_DELAY) != pdTRUE) {
        }
    } else if (xQueueReceive(*mbox, &received, ticks_for(timeout)) != pdTRUE) {
        if (msg != NULL) {
            *msg = NULL;
        }
        return SYS_ARCH_TIMEOUT;
    }
    if (msg != NULL) {
        *msg = received;
    }
    return ms_since(start);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** msg)
{
    void* received = NULL;
    if (xQueueReceive(*mbox, &received, 0) != pdTRUE) {
        return SYS_MBOX_EMPTY;
    }
    if (msg != NULL) {
        *msg = received;
    }
    return 0;
}

void sys_mbox_free(sys_mbox_t* mbox)
{
    vQueueDelete(*mbox);
}

/* A task, of stacksize words, as the SDKs' lwipopts.h have them. */
sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize,
        int prio)
{
    TaskHandle_t task = NULL;
    if (xTaskCreate(thread, name, (uint16_t)stacksize, arg, (UBaseType_t)prio, &task)
            != pdPASS) {
        LWIP_ASSERT("failed to make lwIP's task", 0);
    }
    return task;
}
