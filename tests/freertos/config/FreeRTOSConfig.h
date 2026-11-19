#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS for the port's tests, on the MPS2 AN385 board QEMU emulates. What
   the configurations differ in, CMakeLists.txt defines: TEST_TICK_BITS, 16 or
   32, TEST_TICK_RATE_HZ, and TEST_STATIC, TEST_DYNAMIC and TEST_SUSPEND, 0 or
   1, and TEST_KERNEL_MAJOR, the kernel's major version. See
   https://www.freertos.org/a00110.html for each setting. */

/* The board's Cortex-M3 runs at 25 MHz, which the port sets SysTick from. */
#define configCPU_CLOCK_HZ 25000000
#define configTICK_RATE_HZ TEST_TICK_RATE_HZ
/* The ticks' width: V8 and V10 have no configTICK_TYPE_WIDTH_IN_BITS, and V11
   takes only one of the two. */
#if TEST_KERNEL_MAJOR < 11
#define configUSE_16_BIT_TICKS (TEST_TICK_BITS == 16)
#elif TEST_TICK_BITS == 16
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_16_BITS
#else
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#endif
/* Started 3 seconds before the tick count overflows, for the tests to see the
   monotonic clock go on past it. */
#if TEST_TICK_BITS == 16
#define configINITIAL_TICK_COUNT (0xffffu - 3u * TEST_TICK_RATE_HZ)
#else
#define configINITIAL_TICK_COUNT (0xffffffffu - 3u * TEST_TICK_RATE_HZ)
#endif

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 128
#define configMAX_TASK_NAME_LEN 16
#define configUSE_MUTEXES 1
#define configUSE_TIMERS 0
/* Which V11.1.0 asks for, with configKERNEL_PROVIDED_STATIC_MEMORY, without
   timers too. */
#define configTIMER_TASK_STACK_DEPTH configMINIMAL_STACK_SIZE

/* From V9 on. V8 makes everything from the heap, and has neither: left out,
   as an application's configuration for V8 leaves them out. */
#if TEST_KERNEL_MAJOR >= 9
#define configSUPPORT_STATIC_ALLOCATION TEST_STATIC
#define configSUPPORT_DYNAMIC_ALLOCATION TEST_DYNAMIC
#endif
/* The idle task's memory, without the heap, from V11.1 on. Before, the test
   gives it. */
#define configKERNEL_PROVIDED_STATIC_MEMORY 1
/* heap_4, whose free size tells what the port leaves allocated. */
#define configTOTAL_HEAP_SIZE (64 * 1024)
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
/* Off, as later kernels default them, which V8 has to be told. */
#define configUSE_CO_ROUTINES 0
#define INCLUDE_vTaskPrioritySet 0
#define INCLUDE_vTaskDelayUntil 0
/* Without it, portMAX_DELAY is a timeout like another, which the lock has to
   wait past. */
#define INCLUDE_vTaskSuspend TEST_SUSPEND
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskPriorityGet 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
/* V8's pcTaskGetTaskName(), which V9 has as pcTaskGetName() whatever this says. */
#define INCLUDE_pcTaskGetTaskName 1

#endif /* FREERTOS_CONFIG_H */
