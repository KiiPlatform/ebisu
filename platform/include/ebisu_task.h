/**
 * @file ebisu_task.h
 * @brief Task, delay, lock and monotonic time abstraction.
 */
#ifndef _EBISU_TASK
#define _EBISU_TASK

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
    EBISU_TASKC_OK,
    EBISU_TASKC_FAIL
} ebisu_task_code_t;

/**
 * \brief Function executed in the task.
 */
typedef void* (*EBISU_TASK_ENTRY)(void* value);

/**
 * \brief Callback asks to create task.
 * \param [in] name Name of the task.
 * \param [in] entry Function must be executed by the task.
 * \param [in] entry_param Must be passed to entry as argument.
 * \param [in] userdata Context object pointer.
 */
typedef ebisu_task_code_t
(*EBISU_CB_TASK_CREATE)
    (const char* name,
     EBISU_TASK_ENTRY entry,
     void* entry_param,
     void* userdata);

/**
 * \brief Callback asks to delay/ sleep task.
 * \param [in] msec Time requested to delay/ sleep in milliseconds.
 * \param [in] userdata Context object pointer.
 */
typedef void
(*EBISU_CB_DELAY_MS)
    (uint32_t msec,
     void* userdata);

/**
 * \brief Callback acquires or releases a lock.
 *
 * Set in pairs, one acquiring and one releasing, by kii_session_set_cb_lock().
 * The lock need not be recursive.
 *
 * While it is held, other tasks may be kept waiting for the length of an HTTP
 * request, and the callbacks ebisu makes in between include the socket
 * callbacks. A platform on which no other task using the same session can run
 * while the lock is held -- including inside those callbacks -- may implement
 * both as no-ops.
 *
 * \param [in] userdata Context object pointer.
 */
typedef void
(*EBISU_CB_LOCK)
    (void* userdata);

/**
 * \brief Callback returns a monotonic time in milliseconds.
 *
 * The value must never decrease, and must not jump when the wall clock is
 * changed, for example by NTP. It must keep counting while the device sleeps:
 * a tick counter that stops in low-power modes has tokens renewed late, and
 * ebisu_clock_t falling behind. Its origin is arbitrary; only differences are
 * used. Platforms whose tick counter is 32 bits wide must extend it to 64, or
 * it wraps after about 49.7 days.
 *
 * \param [in] userdata Context object pointer.
 * \return Milliseconds elapsed since an arbitrary, fixed origin.
 */
typedef uint64_t
(*EBISU_CB_MONOTONIC_MS)
    (void* userdata);

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_TASK */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
