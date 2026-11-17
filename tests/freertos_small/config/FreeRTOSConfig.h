#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS for the small tests, on the MPS2 AN385 board QEMU emulates: a task
   for a suite to run in, its stack from malloc(), with heap_3. See
   https://www.freertos.org/a00110.html for each setting. */

/* The board's Cortex-M3 runs at 25 MHz, which the port sets SysTick from. */
#define configCPU_CLOCK_HZ 25000000
#define configTICK_RATE_HZ 1000
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configMAX_PRIORITIES 4
#define configMINIMAL_STACK_SIZE 128
/* In words, for a stack of more than 64K of them. */
#define configSTACK_DEPTH_TYPE uint32_t
#define configMAX_TASK_NAME_LEN 16
#define configUSE_MUTEXES 1
#define configUSE_TIMERS 0

#define configSUPPORT_STATIC_ALLOCATION 0
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configUSE_MALLOC_FAILED_HOOK 0

#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 2

/* As in the sample's. */
#define configKERNEL_INTERRUPT_PRIORITY 0xff
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 5)

void board_assert_failed(const char* file, int line);
#define configASSERT(x) do { if (!(x)) board_assert_failed(__FILE__, __LINE__); } while (0)

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#endif /* FREERTOS_CONFIG_H */
