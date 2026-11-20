/* Runs a suite of tests/large_test in a FreeRTOS task, once the network is up,
   with the arguments QEMU has for it, and ends the program with its status.
   The network and TLS are the FreeRTOS sample's: FreeRTOS-Plus-TCP on the
   board's Ethernet controller, with DHCP, and mbedTLS with the CA certificates
   of its ca.pem. Or, with EBISU_LTEST_LWIP, lwIP on the same controller, and
   mbedTLS 2.4.0, as Realtek's SDKs for AmebaZ and AmebaZ2 configure them, or,
   with EBISU_LTEST_POLARSSL too, PolarSSL 1.3.8, as the SDK for AmebaZ does,
   with the CA certificates of the AmebaZ samples' ca.pem: see lwip/. */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <FreeRTOS.h>
#include <task.h>

#include "catch.hpp"
#include "ltest_platform.h"

extern "C" {
#include "board.h"
#include "log.h"
#ifndef EBISU_LTEST_LWIP
#include "mbedtls_threading.h"
#endif
#include "net.h"
#include "random.h"
/* librdimon's: stdin, stdout and stderr, over semihosting. */
void initialise_monitor_handles(void);
/* From the sample's ca.pem, in ca_pem.c, which CMake generates. */
extern const char ca_pem[];
extern const size_t ca_pem_size;
}

/* The suite's main(), as Catch has it, which CMakeLists.txt renames. */
int ebisu_test_main(int argc, char* argv[]);

/* 256 KB, in words of StackType_t: Catch's frames and the tests', some with
   buffers of 16 KB. A word less before V10, whose stack depth has 16 bits. */
#if tskKERNEL_VERSION_MAJOR < 10
#define SUITE_STACK 0xffffu
#else
#define SUITE_STACK (256 * 1024 / sizeof(StackType_t))
#endif
/* Below FreeRTOS-Plus-TCP's task, as the sample's tasks are; the tasks the
   tests start take this one's. */
#define SUITE_PRIORITY (tskIDLE_PRIORITY + 2)
#define MAX_ARGUMENTS 16

static ebisu::ltest::board_tls_t tls;

ebisu::ltest::board_tls_t* ebisu::ltest::board_tls()
{
    return &tls;
}

#ifdef EBISU_LTEST_POLARSSL
/* Each connection's settings, as the AmebaZ sample's: random_polarssl.c's
   generator, as PolarSSL is built without one, and records no larger than
   the 4096 bytes of those it takes, SSL_MAX_CONTENT_LEN 5120, which the
   server may not honor. */
static void setup_connection(ssl_context* ssl, void* userdata)
{
    (void)userdata;
    ssl_set_rng(ssl, random_tls, NULL);
    (void)ssl_set_max_frag_len(ssl, SSL_MAX_FRAG_LEN_4096);
}
#endif

/* SYS_GET_CMDLINE: the arguments QEMU is given with -semihosting-config
   arg=..., joined with spaces. */
static int semihosting(int operation, const void* arguments)
{
    register int r0 __asm__("r0") = operation;
    register const void* r1 __asm__("r1") = arguments;
    __asm__ volatile("bkpt 0xab" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

/* What the tests take from the environment, CLIENT_ID, CLIENT_SECRET and CI,
   from the lines, NAME=value, of the file EBISU_LTEST_ENV_FILE names on the
   host, through semihosting. They hold secrets: never written anywhere. */
static void read_environment(void)
{
#ifdef EBISU_LTEST_ENV_FILE
    FILE* file = std::fopen(EBISU_LTEST_ENV_FILE, "r");
    if (file == NULL) {
        board_fatal("failed to open EBISU_LTEST_ENV_FILE");
    }
    char line[512];
    while (std::fgets(line, sizeof(line), file) != NULL) {
        line[std::strcspn(line, "\r\n")] = '\0';
        char* equals = std::strchr(line, '=');
        if (equals == NULL) {
            continue;
        }
        *equals = '\0';
        if (setenv(line, equals + 1, 1) != 0) {
            board_fatal("failed to set the environment");
        }
    }
    std::fclose(file);
    std::memset(line, 0, sizeof(line));
    /* Without them, the tests that need them would skip, and pass. */
    if (getenv("CLIENT_ID") == NULL || getenv("CLIENT_SECRET") == NULL) {
        board_fatal("EBISU_LTEST_ENV_FILE has no CLIENT_ID or CLIENT_SECRET");
    }
#endif
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

    read_environment();
    /* Its start, before the tests' tasks ask for it at once. */
    (void)ebisu::ltest::wall_us();

    net_wait_until_up();
#ifdef EBISU_LTEST_POLARSSL
    int ret = ebisu_polarssl_tls_init(&tls, (const unsigned char*)ca_pem, ca_pem_size);
    if (ret != 0) {
        board_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }
    ebisu_polarssl_tls_set_setup(&tls, setup_connection, NULL);
#else
    int ret = ebisu_mbedtls_tls_init(&tls, (const unsigned char*)ca_pem, ca_pem_size);
    if (ret != 0) {
        board_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }
#endif
#if defined(EBISU_LTEST_LWIP) && !defined(EBISU_LTEST_POLARSSL)
    /* As the AmebaZ and AmebaZ2 samples set them up: random.c's generator,
       as mbedTLS is built without one, and records no larger than mbedTLS
       takes, which the server may not honor. */
    mbedtls_ssl_conf_rng(ebisu_mbedtls_tls_config(&tls), random_tls, NULL);
    ret = mbedtls_ssl_conf_max_frag_len(ebisu_mbedtls_tls_config(&tls),
            MBEDTLS_SSL_MAX_FRAG_LEN_4096);
    if (ret != 0) {
        board_fatal("failed to ask for records of 4096 bytes: -0x%04x", (unsigned int)-ret);
    }
#endif

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
    log_init();
#ifdef EBISU_LTEST_LWIP
    int ret = random_init();
    if (ret != 0) {
        board_fatal("failed to seed the random generator: -0x%04x", (unsigned int)-ret);
    }
#else
    /* mbedTLS's mutexes, before PSA's random generator, which TLS and
       FreeRTOS-Plus-TCP take random numbers from, is started. */
    mbedtls_threading_init_freertos();
    int ret = random_init();
    if (ret != 0) {
        board_fatal("failed to start PSA's random generator: %d", ret);
    }
#endif
    if (net_init() != pdPASS) {
        board_fatal("failed to start the network");
    }
    if (xTaskCreate(run_suite, "suite", SUITE_STACK, NULL, SUITE_PRIORITY, NULL) != pdPASS) {
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

extern "C" void vApplicationMallocFailedHook(void)
{
    board_fatal("out of heap");
}
