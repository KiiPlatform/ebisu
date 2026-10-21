#ifndef SYS_CB_WIN32_H
#define SYS_CB_WIN32_H

/* Task callbacks for tio_handler and tio_updater on Windows. */

#include <kii_task_callback.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runs entry(param) on a thread of its own, which nothing waits for. */
kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata);

void delay_ms_cb_impl(uint32_t msec, void* userdata);

/* The session's lock. The handler and the updater run on threads of their
   own, so this is a real lock: userdata is an SRWLOCK. */
void lock_cb_impl(void* userdata);
void unlock_cb_impl(void* userdata);

/* The session's monotonic clock. */
uint64_t monotonic_ms_cb_impl(void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_WIN32_H */
