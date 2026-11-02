#include "ebisu_posix.h"

#include <errno.h>
#include <pthread.h>
#include <time.h>

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    pthread_t pthid;
    if (pthread_create(&pthid, NULL, entry, param) != 0) {
        return EBISU_TASKC_FAIL;
    }
    /* Nothing joins a task, so its thread has to go by itself when it ends. */
    pthread_detach(pthid);
    return EBISU_TASKC_OK;
}

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    struct timespec left;
    left.tv_sec = msec / 1000;
    left.tv_nsec = (long)(msec % 1000) * 1000000;
    /* A signal ends the sleep early, with what was left in left. */
    while (nanosleep(&left, &left) == -1 && errno == EINTR) {
    }
}

static uint64_t monotonic_ms(ebisu_system_t* self)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static const ebisu_system_ops_t posix_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

static ebisu_system_t posix_system = { &posix_system_ops };

ebisu_system_t* ebisu_posix_system(void)
{
    return &posix_system;
}

static void lock(ebisu_lock_t* self)
{
    pthread_mutex_lock(&EBISU_CONTAINER_OF(self, ebisu_posix_lock_t, base)->mutex);
}

static void unlock(ebisu_lock_t* self)
{
    pthread_mutex_unlock(&EBISU_CONTAINER_OF(self, ebisu_posix_lock_t, base)->mutex);
}

static const ebisu_lock_ops_t posix_lock_ops = {
    lock,
    unlock
};

int ebisu_posix_lock_init(ebisu_posix_lock_t* lock)
{
    lock->base.ops = &posix_lock_ops;
    return pthread_mutex_init(&lock->mutex, NULL);
}

void ebisu_posix_lock_destroy(ebisu_posix_lock_t* lock)
{
    pthread_mutex_destroy(&lock->mutex);
}
