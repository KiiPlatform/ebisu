#include "sys_cb_impl.h"
#include "linux-env/task_impl.h"

#include <pthread.h>
#include <time.h>

static uint64_t monotonic_ms(ebisu_system_t* self)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static const ebisu_system_ops_t linux_system_ops = {
    task_create_cb,
    delay_ms_cb,
    monotonic_ms
};

static ebisu_system_t system_ = { &linux_system_ops };

ebisu_system_t* linux_system(void)
{
    return &system_;
}

static void lock(ebisu_lock_t* self)
{
    pthread_mutex_lock(&EBISU_CONTAINER_OF(self, linux_lock_t, base)->mutex);
}

static void unlock(ebisu_lock_t* self)
{
    pthread_mutex_unlock(&EBISU_CONTAINER_OF(self, linux_lock_t, base)->mutex);
}

static const ebisu_lock_ops_t linux_lock_ops = {
    lock,
    unlock
};

void linux_lock_init(linux_lock_t* lock)
{
    lock->base.ops = &linux_lock_ops;
    pthread_mutex_init(&lock->mutex, NULL);
}
