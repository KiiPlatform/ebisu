/* Runs a suite of tests/small_test in a FreeRTOS task, with the arguments QEMU
   has for it, and ends the program with its status. */
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>

#include "catch.hpp"

extern "C" {
#include "board.h"
/* librdimon's: stdin, stdout and stderr, over semihosting. */
void initialise_monitor_handles(void);
}

/* The suite's main(), as Catch has it, which CMakeLists.txt renames. */
int ebisu_test_main(int argc, char* argv[]);

/* 256 KB, in words of StackType_t: Catch's frames and the suites' take tens of
   kilobytes. */
#define SUITE_STACK (256 * 1024 / sizeof(StackType_t))
#define MAX_ARGUMENTS 16

/* SYS_GET_CMDLINE: the arguments QEMU is given with -semihosting-config
   arg=..., joined with spaces. */
static int semihosting(int operation, const void* arguments)
{
    register int r0 __asm__("r0") = operation;
    register const void* r1 __asm__("r1") = arguments;
    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

static void run_suite(void* parameter)
{
    static char line[1024];
    static char* argv[MAX_ARGUMENTS + 1];
    (void)parameter;
    uintptr_t arguments[] = { (uintptr_t)line, sizeof(line) };
    if (semihosting(0x15, arguments) != 0) {
        board_fatal("no command line");
    }
    int argc = 0;
    for (char* word = strtok(line, " "); word != NULL; word = strtok(NULL, " ")) {
        if (argc == MAX_ARGUMENTS) {
            board_fatal("more than %d arguments", MAX_ARGUMENTS);
        }
        argv[argc++] = word;
    }
    argv[argc] = NULL;

    /* Catch passes a run without a test, as a suite would be whose
       constructors didn't register any. */
    if (Catch::getRegistryHub().getTestCaseRegistry().getAllTests().empty()) {
        board_fatal("%s has no test cases", argv[0]);
    }
    int failed = ebisu_test_main(argc, argv);
    std::printf("%s: %lu bytes of its task's stack never used\n", argv[0],
            (unsigned long)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
    std::fflush(stdout);
    std::fflush(stderr);
    board_exit(failed == 0 ? 0 : 1);
}

int main()
{
    board_init();
    initialise_monitor_handles();
    if (xTaskCreate(run_suite, "suite", SUITE_STACK, NULL, 1, NULL) != pdPASS) {
        board_fatal("failed to make the suite's task");
    }
    vTaskStartScheduler();
    board_fatal("failed to start the scheduler");
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    board_fatal("stack overflow in task %s", name);
}

/* The board's vector table has the Ethernet controller's interrupt, which
   FreeRTOS-Plus-TCP's driver handles in the sample. Nothing raises it here. */
extern "C" void EthernetISR(void)
{
}
