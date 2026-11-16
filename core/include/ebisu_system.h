/**
 * @file ebisu_system.h
 * @brief Task, delay, monotonic time and lock abstraction.
 *
 * The inline functions at the end call the operations, so that code given a
 * system or a lock reads as calls: ebisu_system_delay_ms(system, 100) rather
 * than system->ops->delay_ms(system, 100).
 */
#ifndef _EBISU_SYSTEM
#define _EBISU_SYSTEM

#include "ebisu_object.h"

#include <stddef.h>
#include <stdint.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Task codes.
 */
typedef enum ebisu_task_code_t {
    EBISU_TASKC_OK, /**< \brief The task was created. */
    EBISU_TASKC_FAIL /**< \brief The task could not be created. */
} ebisu_task_code_t;

/**
 * \brief Function executed in the task.
 */
typedef void* (*EBISU_TASK_ENTRY)(void* value);

/** \brief The system. See struct ebisu_system_t. */
typedef struct ebisu_system_t ebisu_system_t;

/** \brief A lock. See struct ebisu_lock_t. */
typedef struct ebisu_lock_t ebisu_lock_t;

/** \brief What the system does.
 *
 * Applications implement these for the target environment. See ebisu_object.h
 * for how self leads to the system's state, if it has any.
 */
typedef struct ebisu_system_ops_t {
    /**
     * \brief Creates a task.
     *
     * Needed by whatever starts a task, such as kii_start_push_task(),
     * tio_handler_start() and tio_updater_start().
     *
     * \param [in] self The system.
     * \param [in] name Name of the task.
     * \param [in] entry Function the task must execute.
     * \param [in] entry_param Must be passed to entry as argument.
     */
    ebisu_task_code_t (*task_create)(ebisu_system_t* self, const char* name,
            EBISU_TASK_ENTRY entry, void* entry_param);

    /**
     * \brief Delays the calling task for at least msec milliseconds.
     *
     * Never less, by a monotonic clock: not when a signal interrupts the wait,
     * nor when the system counts the wait in ticks. It may be longer, as the
     * system schedules the task. A delay of 0 may yield to other tasks.
     *
     * \param [in] self The system.
     * \param [in] msec Time requested to delay/ sleep in milliseconds.
     */
    void (*delay_ms)(ebisu_system_t* self, uint32_t msec);

    /**
     * \brief Returns a monotonic time in milliseconds.
     *
     * The value must never decrease, and must not jump when the wall clock is
     * changed, for example by NTP. It must keep counting while the device
     * sleeps: a tick counter that stops in low-power modes has tokens renewed
     * late, and ebisu_clock_t falling behind. Its origin is arbitrary; only
     * differences are used. Platforms whose tick counter is 32 bits wide must
     * extend it to 64, or it wraps after about 49.7 days.
     *
     * More than one task may call it at once: the MQTT task times its
     * keep-alive by it, while a session or an ebisu_clock_t reads it in
     * another task.
     *
     * \param [in] self The system.
     * \return Milliseconds elapsed since an arbitrary, fixed origin.
     */
    uint64_t (*monotonic_ms)(ebisu_system_t* self);

    /**
     * \brief Creates a lock, for code that is given the system and needs a
     * lock of its own.
     *
     * Optional: NULL in a system that can't make one, for which
     * ebisu_system_lock_create() gives NULL. ebisu itself never calls it.
     * Where the lock comes from, the heap or a pool, is the system's.
     *
     * \param [in] self The system.
     * \return The lock, or NULL if none could be made.
     */
    ebisu_lock_t* (*lock_create)(ebisu_system_t* self);

    /**
     * \brief Destroys a lock that lock_create made, which nothing may hold or
     * use any more.
     *
     * Needed with lock_create.
     *
     * \param [in] self The system.
     * \param [in] lock The lock.
     */
    void (*lock_destroy)(ebisu_system_t* self, ebisu_lock_t* lock);
} ebisu_system_ops_t;

/** \brief The system: tasks, delays, a monotonic clock, and maybe locks.
 *
 * One is normally enough for a device, shared by everything it is set on.
 * Embed it in a struct of the application's if it needs state, as
 * ebisu_object.h shows.
 */
struct ebisu_system_t {
    const ebisu_system_ops_t* ops; /**< \brief What the system does. */
};

/** \brief What a lock does.
 *
 * The lock need not be recursive.
 *
 * While it is held, other tasks may be kept waiting for the length of an HTTP
 * request, and the operations ebisu calls in between include those of the
 * sockets. A platform on which no other task using the same session can run
 * while the lock is held -- including inside those operations -- may
 * implement both as no-ops.
 */
typedef struct ebisu_lock_ops_t {
    /**
     * \brief Acquires the lock.
     * \param [in] self The lock.
     */
    void (*lock)(ebisu_lock_t* self);
    /**
     * \brief Releases the lock.
     * \param [in] self The lock.
     */
    void (*unlock)(ebisu_lock_t* self);
} ebisu_lock_ops_t;

/** \brief A lock.
 *
 * Embed it in the struct holding the mutex, as ebisu_object.h shows.
 */
struct ebisu_lock_t {
    const ebisu_lock_ops_t* ops; /**< \brief What the lock does. */
};

/**
 * \brief Delays the calling task, with the system's delay_ms.
 *
 * \param [in] system The system.
 * \param [in] msec How long, in milliseconds.
 */
static inline void ebisu_system_delay_ms(ebisu_system_t* system, uint32_t msec)
{
    system->ops->delay_ms(system, msec);
}

/**
 * \brief The system's monotonic time, with its monotonic_ms.
 *
 * \param [in] system The system.
 * \return Milliseconds since an arbitrary, fixed origin.
 */
static inline uint64_t ebisu_system_monotonic_ms(ebisu_system_t* system)
{
    return system->ops->monotonic_ms(system);
}

/**
 * \brief Creates a lock, with the system's lock_create.
 *
 * \param [in] system The system.
 * \return The lock, to destroy with ebisu_system_lock_destroy(); or NULL if
 * none could be made, or the system has no lock_create.
 */
static inline ebisu_lock_t* ebisu_system_lock_create(ebisu_system_t* system)
{
    return system->ops->lock_create != NULL ? system->ops->lock_create(system) : NULL;
}

/**
 * \brief Destroys a lock that ebisu_system_lock_create() made, with the
 * system's lock_destroy.
 *
 * \param [in] system The system that made it.
 * \param [in] lock The lock, which nothing may hold or use any more. NULL does
 * nothing.
 */
static inline void ebisu_system_lock_destroy(ebisu_system_t* system, ebisu_lock_t* lock)
{
    if (lock != NULL) {
        system->ops->lock_destroy(system, lock);
    }
}

/**
 * \brief Acquires a lock.
 *
 * \param [in] lock The lock. NULL does nothing, as for code that runs in one
 * task and is given no lock.
 */
static inline void ebisu_lock_acquire(ebisu_lock_t* lock)
{
    if (lock != NULL) {
        lock->ops->lock(lock);
    }
}

/**
 * \brief Releases a lock that ebisu_lock_acquire() acquired.
 *
 * \param [in] lock The lock. NULL does nothing.
 */
static inline void ebisu_lock_release(ebisu_lock_t* lock)
{
    if (lock != NULL) {
        lock->ops->unlock(lock);
    }
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_SYSTEM */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
