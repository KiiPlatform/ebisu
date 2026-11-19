/* The tests' lwIP port to a POSIX host: its semaphores, mutexes, mailboxes
   and threads, over POSIX threads, in sys_arch.c. */
#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL NULL

struct sys_sem;
typedef struct sys_sem* sys_sem_t;
#define sys_sem_valid(sem) (((sem) != NULL) && (*(sem) != NULL))
#define sys_sem_set_invalid(sem) do { if ((sem) != NULL) { *(sem) = NULL; } } while (0)

struct sys_mutex;
typedef struct sys_mutex* sys_mutex_t;
#define sys_mutex_valid(mutex) (((mutex) != NULL) && (*(mutex) != NULL))
#define sys_mutex_set_invalid(mutex) do { if ((mutex) != NULL) { *(mutex) = NULL; } } while (0)

struct sys_mbox;
typedef struct sys_mbox* sys_mbox_t;
#define sys_mbox_valid(mbox) (((mbox) != NULL) && (*(mbox) != NULL))
#define sys_mbox_set_invalid(mbox) do { if ((mbox) != NULL) { *(mbox) = NULL; } } while (0)

struct sys_thread;
typedef struct sys_thread* sys_thread_t;

#endif /* LWIP_ARCH_SYS_ARCH_H */
