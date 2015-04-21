#include "kii_task_impl.h"
#include <pthread.h>

kii_task_code_t task_create_cb
    (const char* name,
     KII_TASK_ENTRY entry,
     void* param,
     unsigned char* stk_start,
     unsigned int stk_size,
     unsigned int priority)
{
    int ret;
    pthread_t pthid;

    ret = pthread_create(&pthid, NULL, entry, param);

    if(ret == 0)
    {
        return KII_TASKC_OK;
    }
    else
    {
        return KII_TASKC_FAIL;
    }
}
