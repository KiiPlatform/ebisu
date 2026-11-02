#ifndef SYS_CB_WIN32_H
#define SYS_CB_WIN32_H

/* The system for tio_handler and tio_updater on Windows. */

#include <ebisu_system.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The system: each task runs on a thread of its own, which nothing waits for,
   delays Sleep() and the monotonic clock is GetTickCount64(). It has no state,
   so one serves everything. */
ebisu_system_t* win32_system(void);

/* A lock that is an SRWLOCK. The handler and the updater run on threads of
   their own, so the session's lock has to be a real one. */
typedef struct {
    ebisu_lock_t base;
    SRWLOCK srwlock;
} win32_lock_t;

/* Initializes the SRWLOCK, and makes base a lock of it. */
void win32_lock_init(win32_lock_t* lock);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_WIN32_H */
