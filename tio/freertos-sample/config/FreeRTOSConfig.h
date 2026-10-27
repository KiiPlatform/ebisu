#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS for the sample, on the MPS2 AN385 board QEMU emulates. See
   https://www.freertos.org/a00110.html for each setting. */

/* The board's Cortex-M3 runs at 25 MHz, which the port sets SysTick from. */
#define configCPU_CLOCK_HZ 25000000
#define configTICK_RATE_HZ 1000
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 256
#define configMAX_TASK_NAME_LEN 16

#define configUSE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH 8
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

/* heap_3: pvPortMalloc() is the C library's malloc(), so there is no FreeRTOS
   heap of its own to size. */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configTOTAL_HEAP_SIZE 0

/* Each task its own newlib state, errno among it: jkii checks errno after
   strtoll(), and the handler's and the updater's tasks parse JSON at the same
   time. */
#define configUSE_NEWLIB_REENTRANT 1

#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2

/* Priorities as the NVIC takes them, in the top bits of a byte, of which a
   Cortex-M3 implements 3 to 8: these mean the same with any of them. The
   kernel's interrupts at the lowest, and interrupts that call FreeRTOS, the
   Ethernet controller's, no more urgent than 5 << 5. */
#define configKERNEL_INTERRUPT_PRIORITY 0xff
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 5)

void board_assert_failed(const char* file, int line);
#define configASSERT(x) do { if (!(x)) board_assert_failed(__FILE__, __LINE__); } while (0)

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
/* For portMAX_DELAY to wait for ever. */
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

#endif /* FREERTOS_CONFIG_H */
