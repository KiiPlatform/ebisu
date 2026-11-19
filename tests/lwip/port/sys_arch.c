/* The tests' lwIP port to a POSIX host, over POSIX threads, for lwIP 2.0 to
   2.2 alike: lwIP's own port to one, which 2.2 has in its tree, comes for 2.0
   and 2.1 in a contrib archive of its own. */
#define _POSIX_C_SOURCE 200809L

#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct sys_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    unsigned int count;
};

struct sys_mutex {
    pthread_mutex_t mutex;
};

/* Room for this many messages, whatever size lwIP asks for. */
#define MBOX_ROOM 256

struct sys_mbox {
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    void* msgs[MBOX_ROOM];
    unsigned int first;
    unsigned int count;
};

struct sys_thread {
    pthread_t thread;
    lwip_thread_fn fn;
    void* arg;
};

/* Recursive, made by sys_init(), which lwIP calls before anything else. */
static pthread_mutex_t protect;
static pthread_mutexattr_t recursive;
static struct timespec started;

static void make_cond(pthread_cond_t* cond)
{
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
}

static struct timespec monotonic_now(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now;
}

static u32_t ms_since(const struct timespec* start)
{
    struct timespec now = monotonic_now();
    return (u32_t)((now.tv_sec - start->tv_sec) * 1000 + (now.tv_nsec - start->tv_nsec) / 1000000);
}

static struct timespec deadline_in(u32_t ms)
{
    struct timespec at = monotonic_now();
    at.tv_sec += ms / 1000;
    at.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (at.tv_nsec >= 1000000000L) {
        at.tv_sec += 1;
        at.tv_nsec -= 1000000000L;
    }
    return at;
}

void sys_init(void)
{
    started = monotonic_now();
    pthread_mutexattr_init(&recursive);
    pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&protect, &recursive);
}

u32_t sys_now(void)
{
    return ms_since(&started);
}

u32_t sys_jiffies(void)
{
    return sys_now();
}

sys_prot_t sys_arch_protect(void)
{
    pthread_mutex_lock(&protect);
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    pthread_mutex_unlock(&protect);
}

err_t sys_sem_new(sys_sem_t* sem, u8_t count)
{
    struct sys_sem* s = (struct sys_sem*)calloc(1, sizeof(*s));
    if (s == NULL) {
        return ERR_MEM;
    }
    pthread_mutex_init(&s->mutex, NULL);
    make_cond(&s->cond);
    s->count = count;
    *sem = s;
    return ERR_OK;
}

void sys_sem_signal(sys_sem_t* sem)
{
    struct sys_sem* s = *sem;
    pthread_mutex_lock(&s->mutex);
    ++s->count;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout)
{
    struct sys_sem* s = *sem;
    struct timespec start = monotonic_now();
    struct timespec at = deadline_in(timeout);
    pthread_mutex_lock(&s->mutex);
    while (s->count == 0) {
        if (timeout == 0) {
            pthread_cond_wait(&s->cond, &s->mutex);
        } else if (pthread_cond_timedwait(&s->cond, &s->mutex, &at) == ETIMEDOUT) {
            pthread_mutex_unlock(&s->mutex);
            return SYS_ARCH_TIMEOUT;
        }
    }
    --s->count;
    pthread_mutex_unlock(&s->mutex);
    return ms_since(&start);
}

void sys_sem_free(sys_sem_t* sem)
{
    struct sys_sem* s = *sem;
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);
    free(s);
    *sem = NULL;
}

err_t sys_mutex_new(sys_mutex_t* mutex)
{
    struct sys_mutex* m = (struct sys_mutex*)calloc(1, sizeof(*m));
    if (m == NULL) {
        return ERR_MEM;
    }
    pthread_mutex_init(&m->mutex, NULL);
    *mutex = m;
    return ERR_OK;
}

void sys_mutex_lock(sys_mutex_t* mutex)
{
    pthread_mutex_lock(&(*mutex)->mutex);
}

void sys_mutex_unlock(sys_mutex_t* mutex)
{
    pthread_mutex_unlock(&(*mutex)->mutex);
}

void sys_mutex_free(sys_mutex_t* mutex)
{
    pthread_mutex_destroy(&(*mutex)->mutex);
    free(*mutex);
    *mutex = NULL;
}

err_t sys_mbox_new(sys_mbox_t* mbox, int size)
{
    struct sys_mbox* m;
    (void)size;
    m = (struct sys_mbox*)calloc(1, sizeof(*m));
    if (m == NULL) {
        return ERR_MEM;
    }
    pthread_mutex_init(&m->mutex, NULL);
    make_cond(&m->not_empty);
    make_cond(&m->not_full);
    *mbox = m;
    return ERR_OK;
}

static void put(struct sys_mbox* m, void* msg)
{
    m->msgs[(m->first + m->count) % MBOX_ROOM] = msg;
    ++m->count;
    pthread_cond_signal(&m->not_empty);
}

void sys_mbox_post(sys_mbox_t* mbox, void* msg)
{
    struct sys_mbox* m = *mbox;
    pthread_mutex_lock(&m->mutex);
    while (m->count == MBOX_ROOM) {
        pthread_cond_wait(&m->not_full, &m->mutex);
    }
    put(m, msg);
    pthread_mutex_unlock(&m->mutex);
}

err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg)
{
    struct sys_mbox* m = *mbox;
    err_t ret = ERR_MEM;
    pthread_mutex_lock(&m->mutex);
    if (m->count < MBOX_ROOM) {
        put(m, msg);
        ret = ERR_OK;
    }
    pthread_mutex_unlock(&m->mutex);
    return ret;
}

/* lwIP 2.1 on. A host has no interrupts. */
err_t sys_mbox_trypost_fromisr(sys_mbox_t* mbox, void* msg);
err_t sys_mbox_trypost_fromisr(sys_mbox_t* mbox, void* msg)
{
    return sys_mbox_trypost(mbox, msg);
}

static void* take(struct sys_mbox* m)
{
    void* msg = m->msgs[m->first];
    m->first = (m->first + 1) % MBOX_ROOM;
    --m->count;
    pthread_cond_signal(&m->not_full);
    return msg;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout)
{
    struct sys_mbox* m = *mbox;
    struct timespec start = monotonic_now();
    struct timespec at = deadline_in(timeout);
    void* taken;
    pthread_mutex_lock(&m->mutex);
    while (m->count == 0) {
        if (timeout == 0) {
            pthread_cond_wait(&m->not_empty, &m->mutex);
        } else if (pthread_cond_timedwait(&m->not_empty, &m->mutex, &at) == ETIMEDOUT) {
            pthread_mutex_unlock(&m->mutex);
            if (msg != NULL) {
                *msg = NULL;
            }
            return SYS_ARCH_TIMEOUT;
        }
    }
    taken = take(m);
    pthread_mutex_unlock(&m->mutex);
    if (msg != NULL) {
        *msg = taken;
    }
    return ms_since(&start);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** msg)
{
    struct sys_mbox* m = *mbox;
    void* taken;
    pthread_mutex_lock(&m->mutex);
    if (m->count == 0) {
        pthread_mutex_unlock(&m->mutex);
        return SYS_MBOX_EMPTY;
    }
    taken = take(m);
    pthread_mutex_unlock(&m->mutex);
    if (msg != NULL) {
        *msg = taken;
    }
    return 0;
}

void sys_mbox_free(sys_mbox_t* mbox)
{
    struct sys_mbox* m = *mbox;
    pthread_cond_destroy(&m->not_full);
    pthread_cond_destroy(&m->not_empty);
    pthread_mutex_destroy(&m->mutex);
    free(m);
    *mbox = NULL;
}

static void* thread_main(void* arg)
{
    struct sys_thread* t = (struct sys_thread*)arg;
    t->fn(t->arg);
    return NULL;
}

sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize,
        int prio)
{
    struct sys_thread* t = (struct sys_thread*)calloc(1, sizeof(*t));
    (void)name;
    (void)stacksize;
    (void)prio;
    if (t == NULL) {
        return NULL;
    }
    t->fn = thread;
    t->arg = arg;
    if (pthread_create(&t->thread, NULL, thread_main, t) != 0) {
        free(t);
        return NULL;
    }
    pthread_detach(t->thread);
    return t;
}
