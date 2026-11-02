/**
 * @file sys_cb_impl.h
 * @brief What this sample implements of the system, for Linux.
 */
#ifndef _TIO_ENVIRONMENT_DEPENDENT_
#define _TIO_ENVIRONMENT_DEPENDENT_

#include <ebisu_system.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The system: tasks are threads, delays nanosleep() and the monotonic clock
 * is CLOCK_MONOTONIC. It has no state, so one serves everything. */
ebisu_system_t* linux_system(void);

/** A lock that is a mutex.
 *
 * The handler and the updater run as separate threads, so the session's lock
 * has to be a real one. */
typedef struct {
    ebisu_lock_t base;
    pthread_mutex_t mutex;
} linux_lock_t;

/** Initializes the mutex, and makes base a lock of it. */
void linux_lock_init(linux_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* _TIO_ENVIRONMENT_DEPENDENT_ */
