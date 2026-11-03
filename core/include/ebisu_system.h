/**
 * @file ebisu_system.h
 * @brief Task, delay, monotonic time and lock abstraction.
 */
#ifndef _EBISU_SYSTEM
#define _EBISU_SYSTEM

#include "ebisu_object.h"

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
     * \brief Delays/ sleeps the calling task.
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
} ebisu_system_ops_t;

/** \brief The system: tasks, delays and a monotonic clock.
 *
 * One is normally enough for a device, shared by everything it is set on.
 * Embed it in a struct of the application's if it needs state, as
 * ebisu_object.h shows.
 */
struct ebisu_system_t {
    const ebisu_system_ops_t* ops; /**< \brief What the system does. */
};

/** \brief A lock. See struct ebisu_lock_t. */
typedef struct ebisu_lock_t ebisu_lock_t;

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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_SYSTEM */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
