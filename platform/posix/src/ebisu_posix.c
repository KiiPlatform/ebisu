#include "ebisu_posix.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

static ebisu_task_code_t task_create(
        ebisu_system_t* self,
        const char* name,
        EBISU_TASK_ENTRY entry,
        void* param)
{
    pthread_t pthid;
    if (pthread_create(&pthid, NULL, entry, param) != 0) {
        return EBISU_TASK_FAIL;
    }
    /* Nothing joins a task, so its thread has to go by itself when it ends. */
    pthread_detach(pthid);
    return EBISU_TASK_OK;
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

/* CLOCK_BOOTTIME where the system has it, as Linux since 2.6.39 does:
   CLOCK_MONOTONIC there stops while the system is suspended, which
   ebisu_system_ops_t::monotonic_ms mustn't. Where it doesn't, or the kernel
   refuses it, CLOCK_MONOTONIC, which elsewhere counts through a suspend or
   is all there is. */
static uint64_t monotonic_ms(ebisu_system_t* self)
{
    struct timespec ts;
#ifdef CLOCK_BOOTTIME
    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
    }
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/* The time of day, CLOCK_REALTIME, unless it is one not set. */
static bool clock_now(ebisu_clock_t* self, int64_t* out_now)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ebisu_clock_from_system_time((int64_t)ts.tv_sec, out_now);
}

/* The system keeps the time: none from servers, and nothing to doubt. */
static const ebisu_clock_ops_t posix_clock_ops = {
    clock_now,
    NULL,
    NULL
};

static ebisu_clock_t posix_clock = { &posix_clock_ops };

static ebisu_clock_t* system_clock(ebisu_system_t* self)
{
    return &posix_clock;
}

static ebisu_lock_t* lock_create(ebisu_system_t* self);
static void lock_destroy(ebisu_system_t* self, ebisu_lock_t* lock);

static const ebisu_system_ops_t posix_system_ops = {
    task_create,
    delay_ms,
    monotonic_ms,
    lock_create,
    lock_destroy,
    system_clock
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

bool ebisu_posix_lock_init(ebisu_posix_lock_t* lock)
{
    lock->base.ops = &posix_lock_ops;
    return pthread_mutex_init(&lock->mutex, NULL) == 0;
}

void ebisu_posix_lock_destroy(ebisu_posix_lock_t* lock)
{
    pthread_mutex_destroy(&lock->mutex);
}

/* An ebisu_posix_lock_t of its own, from the heap. */
static ebisu_lock_t* lock_create(ebisu_system_t* self)
{
    ebisu_posix_lock_t* lock = (ebisu_posix_lock_t*)malloc(sizeof(*lock));
    if (lock == NULL) {
        return NULL;
    }
    if (!ebisu_posix_lock_init(lock)) {
        free(lock);
        return NULL;
    }
    return &lock->base;
}

static void lock_destroy(ebisu_system_t* self, ebisu_lock_t* lock)
{
    ebisu_posix_lock_t* posix_lock = EBISU_CONTAINER_OF(lock, ebisu_posix_lock_t, base);
    ebisu_posix_lock_destroy(posix_lock);
    free(posix_lock);
}
