/* The large tests' lwIP port to FreeRTOS: its semaphores, mailboxes and
   threads, FreeRTOS's, in sys_arch.c, and its protection, a critical section.
   Mutexes are semaphores, with LWIP_COMPAT_MUTEX, as the SDKs have them. */
#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL NULL

typedef SemaphoreHandle_t sys_sem_t;
#define sys_sem_valid(sem) (((sem) != NULL) && (*(sem) != NULL))
#define sys_sem_set_invalid(sem) do { if ((sem) != NULL) { *(sem) = NULL; } } while (0)

typedef QueueHandle_t sys_mbox_t;
#define sys_mbox_valid(mbox) (((mbox) != NULL) && (*(mbox) != NULL))
#define sys_mbox_set_invalid(mbox) do { if ((mbox) != NULL) { *(mbox) = NULL; } } while (0)

typedef TaskHandle_t sys_thread_t;

typedef int sys_prot_t;

#endif /* LWIP_ARCH_SYS_ARCH_H */
