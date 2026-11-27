/**
 * @file sys_cb_impl.h
 * @brief The sample's task and delay callbacks, which linux-env/task_impl.c
 * implements with POSIX threads. An application on another system implements
 * them with its own.
 */
#ifndef SYS_CB_IMPL_H
#define SYS_CB_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kii_task_callback.h>

/** Creates a task, given to the handler and the updater with
 * tio_handler_set_cb_task_create() and tio_updater_set_cb_task_create().
 *
 * @param [in] name name of the task.
 * @param [in] entry what the task runs.
 * @param [in] param what entry is given.
 * @param [in] userdata what the setter was given.
 *
 * @return KII_TASKC_OK if the task was created, otherwise KII_TASKC_FAIL.
 */
kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata);

/** Delays the task that calls it for msec milliseconds, through signals,
 * given to the handler and the updater with tio_handler_set_cb_delay_ms() and
 * tio_updater_set_cb_delay_ms().
 *
 * @param [in] msec milliseconds to delay.
 * @param [in] userdata what the setter was given.
 */
void delay_ms_cb_impl(unsigned int msec, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* SYS_CB_IMPL_H */
