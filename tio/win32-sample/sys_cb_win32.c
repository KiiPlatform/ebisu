#include "sys_cb_win32.h"

#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    EBISU_TASK_ENTRY entry;
    void* param;
} task_start_t;

/* _beginthreadex() wants an unsigned __stdcall function, and EBISU_TASK_ENTRY
   is void* (*)(void*). */
static unsigned __stdcall task_main(void* arg)
{
    task_start_t start = *(task_start_t*)arg;
    free(arg);
    start.entry(start.param);
    return 0;
}

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    task_start_t* start = (task_start_t*)malloc(sizeof(*start));
    if (start == NULL) {
        return EBISU_TASKC_FAIL;
    }
    start->entry = entry;
    start->param = param;

    /* _beginthreadex() rather than CreateThread(), so that the C runtime
       sets up what it keeps per thread, which the tasks use through printf
       and the rest of stdio. */
    uintptr_t thread = _beginthreadex(NULL, 0, task_main, start, 0, NULL);
    if (thread == 0) {
        printf("failed to create task %s.\n", name);
        free(start);
        return EBISU_TASKC_FAIL;
    }
    /* The task runs on; the handle is only needed to wait for it, which
       nothing does. The tasks tell the application they have ended through
       their exit callbacks. */
    CloseHandle((HANDLE)thread);
    return EBISU_TASKC_OK;
}

static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    Sleep(msec);
}

/* Milliseconds since the system started, in 64 bits, which do not wrap.
   GetTickCount64() does not jump when the wall clock is set. */
static uint64_t monotonic_ms(ebisu_system_t* self)
{
    return GetTickCount64();
}

static const ebisu_system_ops_t win32_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms
};

static ebisu_system_t system_ = { &win32_system_ops };

ebisu_system_t* win32_system(void)
{
    return &system_;
}

static void lock(ebisu_lock_t* self)
{
    AcquireSRWLockExclusive(&EBISU_CONTAINER_OF(self, win32_lock_t, base)->srwlock);
}

static void unlock(ebisu_lock_t* self)
{
    ReleaseSRWLockExclusive(&EBISU_CONTAINER_OF(self, win32_lock_t, base)->srwlock);
}

static const ebisu_lock_ops_t win32_lock_ops = {
    lock,
    unlock
};

void win32_lock_init(win32_lock_t* lock)
{
    lock->base.ops = &win32_lock_ops;
    InitializeSRWLock(&lock->srwlock);
}
