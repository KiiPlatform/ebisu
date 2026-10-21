#include "sys_cb_win32.h"

#include <windows.h>
#include <process.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    KII_TASK_ENTRY entry;
    void* param;
} task_start_t;

/* _beginthreadex() wants an unsigned __stdcall function, and KII_TASK_ENTRY
   is void* (*)(void*). */
static unsigned __stdcall task_main(void* arg)
{
    task_start_t start = *(task_start_t*)arg;
    free(arg);
    start.entry(start.param);
    return 0;
}

kii_task_code_t task_create_cb_impl(
        const char* name,
        KII_TASK_ENTRY entry,
        void* param,
        void* userdata)
{
    task_start_t* start = (task_start_t*)malloc(sizeof(*start));
    if (start == NULL) {
        return KII_TASKC_FAIL;
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
        return KII_TASKC_FAIL;
    }
    /* The task runs on; the handle is only needed to wait for it, which
       nothing does. The tasks tell the application they have ended through
       their exit callbacks. */
    CloseHandle((HANDLE)thread);
    return KII_TASKC_OK;
}

void delay_ms_cb_impl(uint32_t msec, void* userdata)
{
    Sleep(msec);
}

void lock_cb_impl(void* userdata)
{
    AcquireSRWLockExclusive((PSRWLOCK)userdata);
}

void unlock_cb_impl(void* userdata)
{
    ReleaseSRWLockExclusive((PSRWLOCK)userdata);
}

/* Milliseconds since the system started, in 64 bits, which do not wrap.
   GetTickCount64() does not jump when the wall clock is set. */
uint64_t monotonic_ms_cb_impl(void* userdata)
{
    return GetTickCount64();
}
