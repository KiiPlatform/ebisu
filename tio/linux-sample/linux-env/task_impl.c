#include "task_impl.h"
#include <errno.h>
#include <pthread.h>
#include <time.h>

ebisu_task_code_t task_create_cb(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    int ret;
    pthread_t pthid;

    ret = pthread_create(&pthid, NULL, entry, param);

    if(ret == 0)
    {
        /* Nothing joins a task, so its thread has to go by itself when it
           ends. */
        pthread_detach(pthid);
        return EBISU_TASKC_OK;
    }
    else
    {
        return EBISU_TASKC_FAIL;
    }
}

void delay_ms_cb(ebisu_system_t* self, uint32_t msec)
{
    struct timespec left;
    left.tv_sec = msec / 1000;
    left.tv_nsec = (long)(msec % 1000) * 1000000;
    /* A signal ends the sleep early, with what was left in left. */
    while (nanosleep(&left, &left) == -1 && errno == EINTR) {
    }
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
