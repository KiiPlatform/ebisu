/* What newlib and the C++ library need that librdimon leaves to the program:
   memory for malloc(), a lock around it, and what the start-up files the
   board's start-up code stands in for would have. As the board's syscalls.c
   has the first two, whose other calls librdimon's, over semihosting,
   replace. */
#include <errno.h>
#include <malloc.h>
#include <stddef.h>

#include <FreeRTOS.h>
#include <task.h>

#include "board.h"

/* From mps2_an385.ld: the RAM between the program's data and the stack main()
   starts on, which the interrupts' becomes. */
extern char __heap_start[], __heap_end[];

void* _sbrk(ptrdiff_t increment)
{
    static char* end = __heap_start;
    if (increment > __heap_end - end || increment < __heap_start - end) {
        errno = ENOMEM;
        return (void*)-1;
    }
    char* previous = end;
    end += increment;
    return previous;
}

/* FreeRTOS's heap_3 calls malloc() too. With the scheduler suspended, as
   heap_3 does around it. Never from an interrupt handler. */
void __malloc_lock(struct _reent* reent)
{
    (void)reent;
    vTaskSuspendAll();
}

void __malloc_unlock(struct _reent* reent)
{
    (void)reent;
    (void)xTaskResumeAll();
}

/* For std::random_device: the host's /dev/urandom, as the board reads it. */
int _getentropy(void* buffer, size_t length)
{
    if (board_entropy(buffer, length) != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

/* crtbegin.o's: what the C++ library's destructors of static objects are
   registered for, the program itself. */
void* __dso_handle = &__dso_handle;

/* crtn.o's, which exit() calls after the destructors. Nothing to do. */
void _fini(void)
{
}
