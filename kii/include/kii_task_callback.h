/**
 * @file kii_task_callback.h
 * @brief Callbacks into the tasks kii runs.
 */
#ifndef _KII_TASK_CALLBACK
#define _KII_TASK_CALLBACK

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
 * \brief Bool type definition
 */
typedef enum kii_bool_t
{
    KII_FALSE = 0,
    KII_TRUE
} kii_bool_t;

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

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_CALLBACK  */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
