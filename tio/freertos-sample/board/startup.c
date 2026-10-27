#include <stdint.h>
#include <string.h>

#include "board.h"

/* From mps2_an385.ld. */
extern uint32_t __data_load[], __data_start[], __data_end[];
extern uint32_t __bss_start[], __bss_end[];
extern uint32_t __stack_top[];

int main(void);

/* FreeRTOS's port. */
void vPortSVCHandler(void);
void xPortPendSVHandler(void);
void xPortSysTickHandler(void);
/* FreeRTOS-Plus-TCP's LAN9118 driver. */
void EthernetISR(void);

void reset_handler(void);
void hard_fault_report(const uint32_t* frame);
static void hard_fault_handler(void);
static void unexpected_handler(void);

/* At address 0, where the Cortex-M3 takes its first stack pointer and its
   handlers from: the system's exceptions, then the board's 32 interrupts. */
__attribute__((section(".vectors"), used))
static void (* const vectors[16 + 32])(void) = {
    (void (*)(void))__stack_top,
    reset_handler,
    unexpected_handler, /* NMI */
    hard_fault_handler,
    unexpected_handler, /* memory management fault */
    unexpected_handler, /* bus fault */
    unexpected_handler, /* usage fault */
    0, 0, 0, 0,
    vPortSVCHandler,
    unexpected_handler, /* debug monitor */
    0,
    xPortPendSVHandler,
    xPortSysTickHandler,
    [16 ... 16 + 31] = unexpected_handler,
    [16 + 13] = EthernetISR,
};

void reset_handler(void)
{
    memcpy(__data_start, __data_load, (char*)__data_end - (char*)__data_start);
    memset(__bss_start, 0, (char*)__bss_end - (char*)__bss_start);
    board_exit(main());
}

/* Passes hard_fault_report() the registers the fault stacked, from the stack
   that was in use. */
__attribute__((naked))
static void hard_fault_handler(void)
{
    __asm__ volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "b hard_fault_report\n");
}

void hard_fault_report(const uint32_t* frame)
{
    /* The configurable and the hard fault status registers. */
    uint32_t cfsr = *(volatile uint32_t*)0xE000ED28;
    uint32_t hfsr = *(volatile uint32_t*)0xE000ED2C;
    board_fatal("hard fault at pc 0x%08lx, lr 0x%08lx, cfsr 0x%08lx, hfsr 0x%08lx",
            (unsigned long)frame[6], (unsigned long)frame[5],
            (unsigned long)cfsr, (unsigned long)hfsr);
}

static void unexpected_handler(void)
{
    uint32_t ipsr;
    __asm__ volatile("mrs %0, ipsr" : "=r"(ipsr));
    board_fatal("unexpected exception %lu", (unsigned long)(ipsr & 0x1ff));
}
