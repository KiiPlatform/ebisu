/**
 * @file ebisu_freertos.h
 * @brief The system and a lock on FreeRTOS.
 */
#ifndef _EBISU_FREERTOS
#define _EBISU_FREERTOS

#include <ebisu_system.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The system on FreeRTOS.
 *
 * Each task is a FreeRTOS task of its own, of the stack depth and priority the
 * system is initialized with, made with xTaskCreate(), which deletes itself
 * when it ends. Making one takes from FreeRTOS's heap what xTaskCreate() does,
 * and a few bytes more, given back as the task starts: it needs
 * configSUPPORT_DYNAMIC_ALLOCATION, without which the system makes no tasks,
 * and a heap that frees, as heap_1 doesn't, for a task that ends and starts
 * again, as the MQTT task does when kii_start_push_task() is called again.
 *
 * A delay is vTaskDelay(), for at least the time asked for, and none, for a
 * delay of 0, a yield to the tasks of the same priority.
 *
 * The monotonic clock is FreeRTOS's tick count, with the times it has
 * overflowed, in milliseconds: it doesn't wrap. It counts while the device
 * sleeps as far as the tick count does: with tickless idle, which steps the
 * tick count on waking, but not through a sleep that stops the tick's timer
 * which the port doesn't make up for. Called from tasks, or before the
 * scheduler starts, not from interrupts.
 *
 * Initialize with ebisu_freertos_system_init().
 */
typedef struct ebisu_freertos_system_t {
    ebisu_system_t base; /**< \brief What ebisu is given. */
    configSTACK_DEPTH_TYPE _stack_depth; /**< \private */
    UBaseType_t _priority; /**< \private */
} ebisu_freertos_system_t;

/**
 * \brief Initializes the system, and makes base a system of it.
 *
 * \param [out] system The system.
 * \param [in] stack_depth The stack depth of each task it makes, as
 * xTaskCreate() takes it: in words of StackType_t. A task that makes TLS
 * connections needs the most, for the handshake: the FreeRTOS sample's tasks
 * have 3072 words, 12 KB, with mbedTLS.
 * \param [in] priority The priority of each task it makes. Lower than the
 * TCP/IP stack's task, which its sockets wait for.
 */
void ebisu_freertos_system_init(
        ebisu_freertos_system_t* system,
        configSTACK_DEPTH_TYPE stack_depth,
        UBaseType_t priority);

/**
 * \brief A lock that is a FreeRTOS mutex.
 *
 * With priority inheritance, as FreeRTOS's mutexes have it, and not recursive.
 * Waits for as long as the mutex is held, with or without INCLUDE_vTaskSuspend.
 * Needs configUSE_MUTEXES.
 *
 * Initialize with ebisu_freertos_lock_init().
 */
typedef struct ebisu_freertos_lock_t {
    ebisu_lock_t base; /**< \brief What ebisu is given. */
    SemaphoreHandle_t _mutex; /**< \private */
#if configSUPPORT_STATIC_ALLOCATION == 1
    StaticSemaphore_t _storage; /**< \private */
#endif
} ebisu_freertos_lock_t;

/**
 * \brief Creates the mutex, and makes base a lock of it.
 *
 * In the lock itself, with configSUPPORT_STATIC_ALLOCATION, or else from
 * FreeRTOS's heap. Before the scheduler starts, or from a task.
 *
 * \param [out] lock The lock.
 * \return Whether the mutex was created, which it isn't from a heap without
 * the room.
 */
bool ebisu_freertos_lock_init(ebisu_freertos_lock_t* lock);

/**
 * \brief Deletes the mutex, which nothing may hold or use any more.
 *
 * \param [in,out] lock The lock.
 */
void ebisu_freertos_lock_destroy(ebisu_freertos_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_FREERTOS */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
