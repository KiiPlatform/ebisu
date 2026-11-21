#include "ebisu_win32.h"

#include <windows.h>
#include <process.h>

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
        return EBISU_TASK_FAIL;
    }
    start->entry = entry;
    start->param = param;

    /* _beginthreadex() rather than CreateThread(), so that the C runtime
       sets up what it keeps per thread, which the tasks use through printf
       and the rest of stdio. */
    uintptr_t thread = _beginthreadex(NULL, 0, task_main, start, 0, NULL);
    if (thread == 0) {
        free(start);
        return EBISU_TASK_FAIL;
    }
    /* The task runs on; the handle is only needed to wait for it, which
       nothing does. The tasks tell the application they have ended through
       their exit callbacks. */
    CloseHandle((HANDLE)thread);
    return EBISU_TASK_OK;
}

/* Sleep() counts in the system clock's ticks, of 1 to 15.6 ms, and can end
   part of one early, as wine's can under load. So it is called again for what
   is left until the performance counter, which steady clocks read, has gone
   past the whole time. QueryPerformanceFrequency() can't fail since Windows
   XP. */
static void delay_ms(ebisu_system_t* self, uint32_t msec)
{
    LARGE_INTEGER freq;
    LARGE_INTEGER now;
    LONGLONG deadline;
    LONGLONG wait = msec;
    (void)self;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    /* Rounded up, in two parts so that no frequency overflows it. */
    deadline = now.QuadPart + (LONGLONG)(msec / 1000) * freq.QuadPart +
        ((LONGLONG)(msec % 1000) * freq.QuadPart + 999) / 1000;
    for (;;) {
        LONGLONG left;
        /* INFINITE, 0xFFFFFFFF, would never end. */
        Sleep(wait < INFINITE ? (DWORD)wait : INFINITE - 1);
        QueryPerformanceCounter(&now);
        left = deadline - now.QuadPart;
        if (left <= 0) {
            return;
        }
        /* What is left, in milliseconds rounded up: at least 1. */
        wait = left / freq.QuadPart * 1000 +
            ((left % freq.QuadPart) * 1000 + freq.QuadPart - 1) / freq.QuadPart;
    }
}

/* Milliseconds since the system started, in 64 bits, which do not wrap.
   GetTickCount64() does not jump when the wall clock is set. */
static uint64_t monotonic_ms(ebisu_system_t* self)
{
    return GetTickCount64();
}

/* The time of day, unless it is one not set. A FILETIME counts 100 ns since
   1601-01-01, 11644473600 seconds before the epoch. */
static bool clock_now(ebisu_clock_t* self, int64_t* out_now)
{
    FILETIME ft;
    ULARGE_INTEGER t;
    GetSystemTimeAsFileTime(&ft);
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    return ebisu_clock_from_system_time(
            (int64_t)(t.QuadPart / 10000000) - 11644473600LL, out_now);
}

/* The system keeps the time: none from servers, and nothing to doubt. */
static const ebisu_clock_ops_t win32_clock_ops = {
    clock_now,
    NULL,
    NULL
};

static ebisu_clock_t win32_clock = { &win32_clock_ops };

static ebisu_clock_t* system_clock(ebisu_system_t* self)
{
    return &win32_clock;
}

static ebisu_lock_t* lock_create(ebisu_system_t* self);
static void lock_destroy(ebisu_system_t* self, ebisu_lock_t* lock);

static const ebisu_system_ops_t win32_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms,
    lock_create,
    lock_destroy,
    system_clock
};

static ebisu_system_t system_ = { &win32_system_ops };

ebisu_system_t* ebisu_win32_system(void)
{
    return &system_;
}

static void lock(ebisu_lock_t* self)
{
    AcquireSRWLockExclusive(&EBISU_CONTAINER_OF(self, ebisu_win32_lock_t, base)->srwlock);
}

static void unlock(ebisu_lock_t* self)
{
    ReleaseSRWLockExclusive(&EBISU_CONTAINER_OF(self, ebisu_win32_lock_t, base)->srwlock);
}

static const ebisu_lock_ops_t win32_lock_ops = {
    lock,
    unlock
};

bool ebisu_win32_lock_init(ebisu_win32_lock_t* lock)
{
    lock->base.ops = &win32_lock_ops;
    InitializeSRWLock(&lock->srwlock);
    return true;
}

/* An ebisu_win32_lock_t of its own, from the heap. An SRWLOCK needs nothing
   to be destroyed. */
static ebisu_lock_t* lock_create(ebisu_system_t* self)
{
    ebisu_win32_lock_t* lock = (ebisu_win32_lock_t*)malloc(sizeof(*lock));
    if (lock == NULL) {
        return NULL;
    }
    ebisu_win32_lock_init(lock);
    return &lock->base;
}

static void lock_destroy(ebisu_system_t* self, ebisu_lock_t* lock)
{
    free(EBISU_CONTAINER_OF(lock, ebisu_win32_lock_t, base));
}
