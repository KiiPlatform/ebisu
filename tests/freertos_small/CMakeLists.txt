# ebisu's small tests, the suites tests/small_test defines, each built as a
# firmware for the MPS2 AN385 board QEMU emulates, a Cortex-M3, with newlib and
# its C++ library, which runs the suite in a FreeRTOS task. CTest runs each in
# QEMU. FreeRTOS is built with FreeRTOS-Kernel's own CMake and ebisu with its
# own, as an application builds them. See README.md.
cmake_minimum_required(VERSION 3.24)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(board ${ebisu_root}/tio/freertos-sample/board)

# The FreeRTOS sample's toolchain, for the same board, unless told otherwise.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${ebisu_root}/tio/freertos-sample/arm-none-eabi.cmake)
endif()

project(ebisu_freertos_small_test C CXX)

# The kernel the sample is tested with, the latest, checked against its hash:
# the suites ask nothing of FreeRTOS but a task to run in.
set(kernel V11.3.3)
include(FetchContent)
FetchContent_Declare(freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/${kernel}/FreeRTOS-Kernel${kernel}.zip
    URL_HASH SHA256=dd728be9ecfdcea4121b89ed015dff77eb0d3fa8ce2bff4b660da748b298990f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE config)
set(FREERTOS_PORT GCC_ARM_CM3)
# heap_3: malloc(), which the suites' C++ allocates from too.
set(FREERTOS_HEAP 3)
FetchContent_MakeAvailable(freertos_kernel)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)

# As tests/CMakeLists.txt has them, for tests/small_test's suites, with
# Catch's main() named ebisu_test_main(), which suite_main.cpp calls, and no
# signals to catch.
add_library(ebisu_catch STATIC
    ${ebisu_root}/tests/catch_amalgamated.cpp
    ${ebisu_root}/tests/catch_main.cpp)
target_include_directories(ebisu_catch PUBLIC ${ebisu_root}/tests)
target_compile_features(ebisu_catch PUBLIC cxx_std_14)
target_compile_definitions(ebisu_catch
    PUBLIC CATCH_CONFIG_NO_POSIX_SIGNALS
    PRIVATE CATCH_AMALGAMATED_CUSTOM_MAIN)
set_property(SOURCE ${ebisu_root}/tests/catch_main.cpp APPEND PROPERTY
    COMPILE_DEFINITIONS main=ebisu_test_main)
set(EBISU_JKII_OBJS ebisu_jkii_objs ebisu_core_objs)
set(EBISU_KHC_OBJS ebisu_khc_objs ebisu_core_objs)
set(EBISU_KII_OBJS ebisu_kii_objs ${EBISU_KHC_OBJS} ${EBISU_JKII_OBJS})
list(REMOVE_DUPLICATES EBISU_KII_OBJS)
set(EBISU_TIO_OBJS ebisu_tio_objs ${EBISU_KII_OBJS})
# Threads, which the suites link for the files left out below only.
add_library(Threads::Threads INTERFACE IMPORTED)

# The files of tests/small_test that don't run here, and why.
set(left_out
    # It logs from several std::threads at once.
    core/log_test.cpp
    # It runs tasks on std::thread.
    kii/session_test.cpp)

# What runs a suite: the board, the FreeRTOS task the suite runs in, and the
# C library's calls, over semihosting, for the suites' files and output.
add_library(suite_runner OBJECT
    suite_main.cpp
    syscalls.c
    ${board}/board.c
    ${board}/startup.c)
target_include_directories(suite_runner PRIVATE ${board})
target_compile_options(suite_runner PRIVATE -Wall -Werror)
target_link_libraries(suite_runner PUBLIC freertos_kernel ebisu_catch)

find_program(QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)

# tests/small_test's, for a firmware of each suite: its sources but those left
# out, run in QEMU in the suite's working directory, which semihosting opens
# the suite's files in.
function(ebisu_add_test_suite name)
    cmake_parse_arguments(T "" "LABEL;WORKING_DIR" "SOURCES;LINK;INCLUDES;DEFINES" ${ARGN})
    set(sources "")
    foreach(source IN LISTS T_SOURCES)
        if(NOT source IN_LIST left_out)
            list(APPEND sources ${source})
        endif()
    endforeach()

    add_executable(${name} ${sources})
    set_target_properties(${name} PROPERTIES
        SUFFIX .elf
        LINK_DEPENDS ${board}/mps2_an385.ld)
    target_compile_features(${name} PRIVATE cxx_std_14)
    target_compile_options(${name} PRIVATE -Wall)
    if(T_DEFINES)
        target_compile_definitions(${name} PRIVATE ${T_DEFINES})
    endif()
    target_link_libraries(${name} PRIVATE suite_runner ebisu_catch ${T_LINK})
    if(T_INCLUDES)
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
    endif()
    # The board's start-up code starts the program, not the C library's;
    # librdimon has the C library's calls over semihosting.
    target_link_options(${name} PRIVATE
        -nostartfiles
        --specs=rdimon.specs
        -T ${board}/mps2_an385.ld
        -Wl,--gc-sections)

    # With Catch's console reporter: its JUnit one keeps every assertion's
    # result until the end, more than the board's memory holds for some
    # suites. The suite's name is its first argument, as a program's is. No
    # colours: Catch can't tell semihosting's output isn't a terminal.
    add_test(NAME ${name}
        COMMAND ${QEMU_SYSTEM_ARM} -machine mps2-an385 -cpu cortex-m3
            -display none -monitor none -serial stdio
            -semihosting-config enable=on,target=native,arg=${name},arg=--colour-mode,arg=none
            -kernel $<TARGET_FILE:${name}>
        WORKING_DIRECTORY ${T_WORKING_DIR})
    set_tests_properties(${name} PROPERTIES LABELS "${T_LABEL}" TIMEOUT 600)
endfunction()

enable_testing()
# tests/small_test's paths start at ebisu's root, its project's directory.
set(PROJECT_SOURCE_DIR ${ebisu_root})
add_subdirectory(${ebisu_root}/tests/small_test small_test)

# The way a suite fails, which the suites passing never take: Catch fails a
# run that runs no test case, as one whose test case it can't find, for the
# firmware to end QEMU with 1.
add_test(NAME small_test_tio_fails
    COMMAND ${QEMU_SYSTEM_ARM} -machine mps2-an385 -cpu cortex-m3
        -display none -monitor none -serial stdio
        -semihosting-config enable=on,target=native,arg=small_test_tio,arg=--colour-mode,arg=none,arg=no-such-test
        -kernel $<TARGET_FILE:small_test_tio>
    WORKING_DIRECTORY ${ebisu_root}/tests/small_test/tio)
set_tests_properties(small_test_tio_fails PROPERTIES LABELS small TIMEOUT 60 WILL_FAIL TRUE)
