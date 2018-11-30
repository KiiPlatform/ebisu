/**
 * @file tio_environment_dependent.h
 * @brief This is a file defining environment dependent functions.
 * this SDKrequires to implement these functions in each target
 * environment.
 */
#ifndef _TIO_ENVIRONMENT_DEPENDENT_
#define _TIO_ENVIRONMENT_DEPENDENT_

#ifdef __cplusplus
extern "C" {
#endif

#include <kii_task_callback.h>

/** Implementation of callback to create task.
 * this SDK requirest to implement this function in each
 * target environment.
 *
 * This function is assigned to fields
 * kii_t#kii_core_t#kii_http_context_t#kii_http_context_t#task_create_cb
 * of command handler and state updater.
 *
 * @param [in] name name of task.
 * @param [in] entry entry of task.
 * @param [in] stk_start start position of stack area.
 * @param [in] stk_size stack size of task
 * @param [in] priority priority of thisk
 *
 * @return KII_TASKC_OK if succeed to create task. otherwise KII_TASKC_FAIL.
 */
kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata);

/** Implementation of callback to delay task.
 * this SDK requirest to implement this function in each
 * target environment.
 *
 * This function is assigned to fields
 * kii_t#kii_core_t#kii_http_context_t#kii_http_context_t#delay_ms_cb
 * of command handler and state updater.
 *
 * @param[in] msec millisecond to delay.
 */
void delay_ms_cb_impl(unsigned int msec, void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _TIO_ENVIRONMENT_DEPENDENT_ */
