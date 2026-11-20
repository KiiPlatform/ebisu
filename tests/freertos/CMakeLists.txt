# The FreeRTOS port's tests: a firmware for the MPS2 AN385 board QEMU emulates,
# a Cortex-M3, which CTest runs in QEMU. FreeRTOS is built with FreeRTOS-Kernel's
# own CMake and ebisu with its own, as an application builds them. See
# README.md.
cmake_minimum_required(VERSION 3.24)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(board ${ebisu_root}/tio/freertos-sample/board)

# The FreeRTOS sample's toolchain, for the same board, unless told otherwise.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${ebisu_root}/tio/freertos-sample/arm-none-eabi.cmake)
endif()

project(ebisu_freertos_test C)

set(EBISU_TEST_KERNEL V11.3.3 CACHE STRING
    "The FreeRTOS kernel: V8.1.2, the oldest the port supports, V9.0.0, V10.0.1, V11.1.0, FreeRTOS LTS 202406's, or V11.3.3, the latest")
set(EBISU_TEST_CONFIG a CACHE STRING
    "What FreeRTOS is configured with: a, b, c or d, below")
include(${CMAKE_CURRENT_LIST_DIR}/kernel.cmake)
if(NOT DEFINED ebisu_kernel_sha256_${EBISU_TEST_KERNEL})
    message(FATAL_ERROR "No such kernel to test with: ${EBISU_TEST_KERNEL}")
endif()

# Ticks of 32 bits at 1 kHz with both kinds of allocation; of 16 bits at
# 250 Hz, a rate that isn't a divisor of 1000, with the heap only, and without
# INCLUDE_vTaskSuspend, so that portMAX_DELAY is a timeout of 65535 ticks; of
# 32 bits at 100 Hz without a heap; and of 32 bits at 1 kHz with the heap only.
if(EBISU_TEST_CONFIG STREQUAL "a")
    set(config TEST_TICK_BITS=32 TEST_TICK_RATE_HZ=1000 TEST_STATIC=1 TEST_DYNAMIC=1 TEST_SUSPEND=1)
elseif(EBISU_TEST_CONFIG STREQUAL "b")
    set(config TEST_TICK_BITS=16 TEST_TICK_RATE_HZ=250 TEST_STATIC=0 TEST_DYNAMIC=1 TEST_SUSPEND=0)
elseif(EBISU_TEST_CONFIG STREQUAL "c")
    set(config TEST_TICK_BITS=32 TEST_TICK_RATE_HZ=100 TEST_STATIC=1 TEST_DYNAMIC=0 TEST_SUSPEND=1)
elseif(EBISU_TEST_CONFIG STREQUAL "d")
    set(config TEST_TICK_BITS=32 TEST_TICK_RATE_HZ=1000 TEST_STATIC=0 TEST_DYNAMIC=1 TEST_SUSPEND=1)
else()
    message(FATAL_ERROR "No such configuration: ${EBISU_TEST_CONFIG}")
endif()
string(REGEX MATCH "^V([0-9]+)\\." major ${EBISU_TEST_KERNEL})
set(major ${CMAKE_MATCH_1})
list(APPEND config TEST_KERNEL_MAJOR=${major})
# V8 makes everything from the heap: no static allocation to test.
if(major LESS 9 AND "TEST_STATIC=1" IN_LIST config)
    message(FATAL_ERROR "${EBISU_TEST_KERNEL} has no static allocation: configuration b or d")
endif()
# What FreeRTOS-Kernel's CMake asks of an application: its FreeRTOSConfig.h,
# and the heap, if any. kernel.cmake has the kernel's releases, and builds
# those before V11, which have no CMake of their own.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE config)
target_compile_definitions(freertos_config INTERFACE ${config})
if("TEST_DYNAMIC=1" IN_LIST config)
    set(FREERTOS_HEAP 4)
endif()
ebisu_test_freertos_kernel(${EBISU_TEST_KERNEL})

# ebisu, with its port to FreeRTOS, which its CMake builds as the kernel's
# target is there.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)

add_executable(freertos_port_test
    freertos_port_test.c
    ${board}/board.c
    ${board}/startup.c
    ${board}/syscalls.c)
set_target_properties(freertos_port_test PROPERTIES
    SUFFIX .elf
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    LINK_DEPENDS ${board}/mps2_an385.ld)
target_include_directories(freertos_port_test PRIVATE ${board})
target_compile_options(freertos_port_test PRIVATE -Wall -Werror)
target_link_libraries(freertos_port_test PRIVATE ebisu_freertos)
# The board's start-up code starts the program, not the C library's.
target_link_options(freertos_port_test PRIVATE
    -nostartfiles
    -T ${board}/mps2_an385.ld
    -Wl,--gc-sections)

enable_testing()
find_program(QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)
add_test(NAME freertos_port
    COMMAND ${QEMU_SYSTEM_ARM} -machine mps2-an385 -cpu cortex-m3
        -display none -monitor none -serial stdio
        -semihosting-config enable=on,target=native
        -kernel $<TARGET_FILE:freertos_port_test>)
set_tests_properties(freertos_port PROPERTIES TIMEOUT 120)
