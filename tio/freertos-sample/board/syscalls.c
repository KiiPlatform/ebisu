/* What newlib needs of the system: memory for malloc(), a lock around it, and
   the rest, which the sample does not use but newlib refers to. The sample
   formats into buffers and writes them with board_write(), never through
   stdio. */
#include <errno.h>
#include <malloc.h>
#include <stddef.h>
#include <sys/stat.h>

#include <FreeRTOS.h>
#include <task.h>

#include "board.h"

/* From mps2_an385.ld: the RAM left between the program's data and the stack
   main() starts on, which the interrupts' becomes. */
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

/* Every task allocates: FreeRTOS's heap_3, mbedTLS and ebisu all call
   malloc(). With the scheduler suspended, as heap_3 does around it, one at a
   time. Never from an interrupt handler. */
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

int _write(int file, const char* data, int length)
{
    (void)file;
    board_write(data, (size_t)length);
    return length;
}

int _read(int file, char* data, int length)
{
    (void)file;
    (void)data;
    (void)length;
    errno = ENOSYS;
    return -1;
}

int _close(int file)
{
    (void)file;
    errno = EBADF;
    return -1;
}

int _fstat(int file, struct stat* status)
{
    (void)file;
    status->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int offset, int whence)
{
    (void)file;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int signal)
{
    (void)pid;
    (void)signal;
    errno = EINVAL;
    return -1;
}

void _exit(int status)
{
    board_exit(status);
}
