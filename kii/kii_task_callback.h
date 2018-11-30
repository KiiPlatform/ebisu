#ifndef _KII_TASK_CALLBACK
#define _KII_TASK_CALLBACK

#ifdef __cplusplus
extern "C" {
#endif

typedef enum kii_task_code_t {
    KII_TASKC_OK,
    KII_TASKC_FAIL
} kii_task_code_t;

/** bool type definition */
typedef enum kii_bool_t
{
    KII_FALSE = 0,
    KII_TRUE
} kii_bool_t;

typedef void* (*KII_TASK_ENTRY)(void* value);

typedef kii_task_code_t
(*KII_CB_TASK_CREATE)
    (const char* name,
     KII_TASK_ENTRY entry,
     void* entry_param,
     void* userdata);

typedef void
(*KII_CB_DELAY_MS)
    (unsigned int msec,
     void* userdata);

typedef void
(*KII_CB_TASK_EXIT)
    (void* task_info,
    void* userdata);

typedef kii_bool_t
(*KII_TASK_CONTINUE)
    (void* task_info,
    void* userdata);

#ifdef __cplusplus
}
#endif

#endif /* _KII_TASK_CALLBACK  */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
