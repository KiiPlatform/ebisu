/**
 * @file kii_task_callback.h
 * @brief Task/ Thread abstraction.
 */
#ifndef _KII_TASK_CALLBACK
#define _KII_TASK_CALLBACK

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Task codes.
 */
typedef enum kii_task_code_t {
    KII_TASKC_OK,
    KII_TASKC_FAIL
} kii_task_code_t;

/**
 * \brief Bool type definition
 */
typedef enum kii_bool_t
{
    KII_FALSE = 0,
    KII_TRUE
} kii_bool_t;

/**
 * \brief Function executed in the task.
 */
typedef void* (*KII_TASK_ENTRY)(void* value);

/**
 * \brief Callback asks to create task.
 * \param [in] name Name of the task.
 * \param [in] entry Function must be executed by the task.
 * \param [in] entry_param Must be passed to entry as argument.
 * \param [in] userdata Context object pointer.
 */
typedef kii_task_code_t
(*KII_CB_TASK_CREATE)
    (const char* name,
     KII_TASK_ENTRY entry,
     void* entry_param,
     void* userdata);

/**
 * \brief Callback asks to delay/ sleep task.
 * \param [in] msec Time requested to delay/ sleep in milliseconds.
 * \param [in] userdata Context object pointer.
 */
typedef void
(*KII_CB_DELAY_MS)
    (unsigned int msec,
     void* userdata);

/**
 * \brief Callback called right before exit of the task.
 *
 * Implementation can execute cleanup process here.
 * \param [in] task_info Information of task.
 * The type of this parameter is defined by the module requires task execution.
 * \param [in] userdata Context object pointer.
 */
typedef void
(*KII_CB_TASK_EXIT)
    (void* task_info,
    void* userdata);

/**
 * \brief Callback determines whether to continue task execution.
 *
 * If discontinued, KII_CB_TASK_EXIT callback is called.
 * \param [in] task_info Information of task.
 * The type of this parameter is defined by the module requires task execution.
 * \param [in] userdata Context object pointer.
 * \return KII_TRUE continues task execution, returning Kii_FALSE results task exit.
 */
typedef kii_bool_t
(*KII_CB_TASK_CONTINUE)
    (void* task_info,
    void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_CALLBACK  */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
