/**
 * @file ebisu_freertos.h
 * @brief The system and a lock on FreeRTOS.
 */
#ifndef EBISU_FREERTOS_H
#define EBISU_FREERTOS_H

#include <ebisu_system.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The type of a task's stack depth, as xTaskCreate() takes it:
 * configSTACK_DEPTH_TYPE, or uint16_t before V10, which has none.
 */
#ifdef configSTACK_DEPTH_TYPE
#define EBISU_FREERTOS_STACK_DEPTH_TYPE configSTACK_DEPTH_TYPE
#else
#define EBISU_FREERTOS_STACK_DEPTH_TYPE uint16_t
#endif

/**
 * \brief Whether FreeRTOS makes tasks and mutexes from its heap: as
 * configSUPPORT_DYNAMIC_ALLOCATION says, or, before V9, which has no such
 * setting and makes them so alone, 1.
 */
#ifndef configSUPPORT_DYNAMIC_ALLOCATION
#define EBISU_FREERTOS_DYNAMIC_ALLOCATION 1
#elif configSUPPORT_DYNAMIC_ALLOCATION == 1
#define EBISU_FREERTOS_DYNAMIC_ALLOCATION 1
#else
#define EBISU_FREERTOS_DYNAMIC_ALLOCATION 0
#endif

/**
 * \brief Whether FreeRTOS makes mutexes in memory it is given: as
 * configSUPPORT_STATIC_ALLOCATION says, or, before V9, which can't, 0.
 */
#ifndef configSUPPORT_STATIC_ALLOCATION
#define EBISU_FREERTOS_STATIC_ALLOCATION 0
#elif configSUPPORT_STATIC_ALLOCATION == 1
#define EBISU_FREERTOS_STATIC_ALLOCATION 1
#else
#define EBISU_FREERTOS_STATIC_ALLOCATION 0
#endif

/**
 * \brief The system on FreeRTOS.
 *
 * Each task is a FreeRTOS task of its own, of the stack depth and priority the
 * system is initialized with, made with xTaskCreate(), which deletes itself
 * when it ends. Making one takes from FreeRTOS's heap what xTaskCreate() does,
 * and a few bytes more, given back as the task starts: it needs
 * EBISU_FREERTOS_DYNAMIC_ALLOCATION, without which the system makes no tasks,
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
 * Its locks, from ebisu_system_lock_create(), are ebisu_freertos_lock_t's from
 * FreeRTOS's heap, with their mutexes in them with
 * EBISU_FREERTOS_STATIC_ALLOCATION. Without EBISU_FREERTOS_DYNAMIC_ALLOCATION
 * it makes none, and ebisu_system_lock_create() gives NULL.
 *
 * FreeRTOS keeps no time of day, so the system's clock, ebisu_system_clock()'s,
 * is the one the application keeps and sets with
 * ebisu_freertos_system_set_clock(), and none until then.
 *
 * Initialize with ebisu_freertos_system_init().
 */
typedef struct ebisu_freertos_system_t {
    ebisu_system_t base; /**< \brief What ebisu is given. */
    EBISU_FREERTOS_STACK_DEPTH_TYPE _stack_depth; /**< \private */
    UBaseType_t _priority; /**< \private */
    ebisu_clock_t* _clock; /**< \private */
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
        EBISU_FREERTOS_STACK_DEPTH_TYPE stack_depth,
        UBaseType_t priority);

/**
 * \brief Set the clock the system gives, as ebisu_system_clock().
 *
 * FreeRTOS keeps no time of day, so the system gives the one the application
 * keeps, for code given only the system to find:
 * - an ebisu_server_clock_t over this system, for a board without a real-time
 *   clock: the time servers state over TLS, or a time source of the
 *   application's gives it, counted on with the tick count;
 * - or a clock of the application's own, implementing ebisu_clock_ops_t over a
 *   board's real-time clock, with ebisu_clock_from_system_time(), or over a
 *   time source of its own, an NTP client that authenticates its servers for
 *   instance, which may keep the time it is given in an ebisu_server_clock_t.
 *   The kii README's section on the time shows one.
 *
 * ebisu itself never asks the system for its clock: set the clock on the
 * sockets, and a clock that takes the time servers state on each kii_t, too.
 * Set it before tasks other than the caller's may ask for it: the system
 * doesn't guard it.
 *
 * \param [out] system The system.
 * \param [in] clock The clock, which has to outlive the system's use of it; or
 * NULL for none, as the system has until this sets one.
 */
void ebisu_freertos_system_set_clock(ebisu_freertos_system_t* system, ebisu_clock_t* clock);

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
#if EBISU_FREERTOS_STATIC_ALLOCATION
    StaticSemaphore_t _storage; /**< \private */
#endif
} ebisu_freertos_lock_t;

/**
 * \brief Creates the mutex, and makes base a lock of it.
 *
 * In the lock itself, with EBISU_FREERTOS_STATIC_ALLOCATION, or else from
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

#endif /* EBISU_FREERTOS_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
