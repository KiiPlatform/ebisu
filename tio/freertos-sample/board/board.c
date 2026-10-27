#include "board.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

/* The console: UART0, a CMSDK APB UART. */
typedef struct {
    volatile uint32_t data;
    volatile uint32_t state;
    volatile uint32_t ctrl;
    volatile uint32_t intstatus;
    volatile uint32_t bauddiv;
} uart_t;
#define UART0 ((uart_t*)0x40004000)
#define UART_STATE_TX_FULL 1u
#define UART_CTRL_TX_ENABLE 1u

/* The NVIC's priorities, a byte for each interrupt. */
#define NVIC_IPR ((volatile uint8_t*)0xE000E400)
/* The LAN9118's interrupt, which FreeRTOS-Plus-TCP's driver enables. */
#define ETHERNET_IRQ 13

void board_init(void)
{
    /* The lowest divisor the UART takes. */
    UART0->bauddiv = 16;
    UART0->ctrl = UART_CTRL_TX_ENABLE;
    /* The driver's handler calls FreeRTOS, so it cannot be more urgent than
       configMAX_SYSCALL_INTERRUPT_PRIORITY: the least urgent of all. */
    NVIC_IPR[ETHERNET_IRQ] = 0xff;
}

void board_write(const char* data, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        while (UART0->state & UART_STATE_TX_FULL) {
        }
        UART0->data = (unsigned char)data[i];
    }
}

/* Arm semihosting: the debugger does what the program asks, QEMU here with
   -semihosting-config enable=on,target=native. On a board without a debugger
   attached, the breakpoint faults. */
enum {
    SYS_OPEN = 0x01,
    SYS_CLOSE = 0x02,
    SYS_READ = 0x06,
    SYS_EXIT = 0x18
};

static int semihosting(int operation, const void* arguments)
{
    register int r0 __asm__("r0") = operation;
    register const void* r1 __asm__("r1") = arguments;
    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

int board_entropy(unsigned char* buffer, size_t length)
{
    static const char path[] = "/dev/urandom";
    /* Mode 1 is "rb". */
    const uintptr_t open_arguments[] = { (uintptr_t)path, 1, sizeof(path) - 1 };
    int handle = semihosting(SYS_OPEN, open_arguments);
    if (handle == -1) {
        return -1;
    }
    const uintptr_t read_arguments[] = { (uintptr_t)handle, (uintptr_t)buffer, length };
    /* How many bytes it did not read. */
    int unread = semihosting(SYS_READ, read_arguments);
    const uintptr_t close_arguments[] = { (uintptr_t)handle };
    semihosting(SYS_CLOSE, close_arguments);
    return unread == 0 ? 0 : -1;
}

void board_exit(int status)
{
    /* On 32-bit Arm, SYS_EXIT takes the reason itself: QEMU exits with 0 for
       ADP_Stopped_ApplicationExit, and 1 for any other, such as
       ADP_Stopped_RunTimeErrorUnknown. */
    semihosting(SYS_EXIT, (const void*)(uintptr_t)(status == 0 ? 0x20026 : 0x20023));
    for (;;) {
    }
}

void board_fatal(const char* format, ...)
{
    /* Nothing else runs from here on. */
    __asm__ volatile("cpsid i" ::: "memory");
    static const char prefix[] = "FATAL: ";
    static char message[192];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (length < 0) {
        length = 0;
    } else if ((size_t)length >= sizeof(message)) {
        length = sizeof(message) - 1;
    }
    board_write(prefix, sizeof(prefix) - 1);
    board_write(message, (size_t)length);
    board_write("\r\n", 2);
    board_exit(1);
}

/* configASSERT(), in FreeRTOSConfig.h. */
void board_assert_failed(const char* file, int line)
{
    board_fatal("assert failed at %s:%d", file, line);
}
