#include "sys_cb_impl.h"
#include "linux-env/task_impl.h"

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>

void delay_ms_cb_impl(uint32_t msec, void* userdata)
{
    delay_ms_cb(msec, userdata);
}

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    return task_create_cb(name, entry, param, userdata);
}

void lock_cb_impl(void* userdata)
{
    pthread_mutex_lock((pthread_mutex_t*)userdata);
}

void unlock_cb_impl(void* userdata)
{
    pthread_mutex_unlock((pthread_mutex_t*)userdata);
}

uint64_t monotonic_ms_cb_impl(void* userdata)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
