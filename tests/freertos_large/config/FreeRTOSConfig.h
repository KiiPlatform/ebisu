#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* FreeRTOS for the large tests, on the MPS2 AN385 board QEMU emulates: the
   FreeRTOS sample's, ../../../tio/freertos-sample/config/FreeRTOSConfig.h,
   with stacks of more than 64K words, for the suite's task, and a task's
   priority to read, for the tests' tasks to take. See
   https://www.freertos.org/a00110.html for each setting. */

/* The board's Cortex-M3 runs at 25 MHz, which the port sets SysTick from. */
#define configCPU_CLOCK_HZ 25000000
#define configTICK_RATE_HZ 1000
/* Ticks of 32 bits: V8 and V10 have no configTICK_TYPE_WIDTH_IN_BITS, which
   V11 has in place of configUSE_16_BIT_TICKS. CMakeLists.txt defines
   TEST_KERNEL_MAJOR, the kernel's major version. */
#if TEST_KERNEL_MAJOR < 11
#define configUSE_16_BIT_TICKS 0
#else
#define configTICK_TYPE_WIDTH_IN_BITS TICK_TYPE_WIDTH_32_BITS
#endif

#define configUSE_PREEMPTION 1
#define configUSE_TIME_SLICING 1
#define configMAX_PRIORITIES 8
#define configMINIMAL_STACK_SIZE 256
#define configSTACK_DEPTH_TYPE uint32_t
#define configMAX_TASK_NAME_LEN 16

#define configUSE_MUTEXES 1
#define configUSE_COUNTING_SEMAPHORES 1
#define configUSE_TIMERS 1
#define configTIMER_TASK_PRIORITY (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH 8
#define configTIMER_TASK_STACK_DEPTH (configMINIMAL_STACK_SIZE * 2)

/* heap_3: pvPortMalloc() is the C library's malloc(), which mbedTLS and the
   tests' C++ allocate with too. */
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configSUPPORT_STATIC_ALLOCATION 0
#define configTOTAL_HEAP_SIZE 0

/* Each task its own newlib state, which newlib's strtod(), as jkii reads
   doubles with it, works in: the tasks the tests start parse JSON at the same
   time. */
#define configUSE_NEWLIB_REENTRANT 1

#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configUSE_MALLOC_FAILED_HOOK 1
#define configCHECK_FOR_STACK_OVERFLOW 2

/* As in the sample's. */
#define configKERNEL_INTERRUPT_PRIORITY 0xff
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 5)

/* board.c's, which C++ calls too. */
#ifdef __cplusplus
extern "C"
#endif
void board_assert_failed(const char* file, int line);
#define configASSERT(x) do { if (!(x)) board_assert_failed(__FILE__, __LINE__); } while (0)

#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskDelete 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_xTaskGetSchedulerState 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_uxTaskPriorityGet 1
/* Off, as later kernels default them, which V8 has to be told. */
#define configUSE_CO_ROUTINES 0
#define INCLUDE_vTaskPrioritySet 0
#define INCLUDE_vTaskDelayUntil 0

#endif /* FREERTOS_CONFIG_H */
