# The tests of the socket over FreeRTOS-Plus-TCP: a firmware for the MPS2 AN385
# board QEMU emulates, which CTest runs in QEMU with run_tcp_test.py, against
# its servers on the host. FreeRTOS and FreeRTOS-Plus-TCP are built with their
# own CMake, and ebisu with its own, as an application builds them. See
# README.md.
cmake_minimum_required(VERSION 3.24)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(board ${ebisu_root}/tio/freertos-sample/board)

# The FreeRTOS sample's toolchain, for the same board, unless told otherwise.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${ebisu_root}/tio/freertos-sample/arm-none-eabi.cmake)
endif()

project(ebisu_freertos_tcp_test C)

set(EBISU_TEST_KERNEL V11.3.3 CACHE STRING
    "The FreeRTOS kernel: V11.1.0, FreeRTOS LTS 202406's, or V11.3.3, the latest")
set(EBISU_TEST_PLUS_TCP V4.4.2 CACHE STRING
    "FreeRTOS-Plus-TCP: V4.2.6, FreeRTOS LTS 202406's, or V4.4.2, the latest")

# The releases, checked against their hashes.
set(kernel_sha256_V11.1.0 eebd58aa71a623c9381f25f77b708c0ed14ef995a8913e2460fe9f286bb271eb)
set(kernel_sha256_V11.3.3 dd728be9ecfdcea4121b89ed015dff77eb0d3fa8ce2bff4b660da748b298990f)
set(plus_tcp_sha256_V4.2.6 76190ec494c584104bec69fcfa7b17868cb3691fff2e133bf18eed557e7a6145)
set(plus_tcp_sha256_V4.4.2 bea95f97c6eecfc957e3fba411a21055ef9f26906b3268bee911a7d2756bf181)
if(NOT DEFINED kernel_sha256_${EBISU_TEST_KERNEL})
    message(FATAL_ERROR "No such kernel to test with: ${EBISU_TEST_KERNEL}")
endif()
if(NOT DEFINED plus_tcp_sha256_${EBISU_TEST_PLUS_TCP})
    message(FATAL_ERROR "No such FreeRTOS-Plus-TCP to test with: ${EBISU_TEST_PLUS_TCP}")
endif()

include(FetchContent)
FetchContent_Declare(freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/${EBISU_TEST_KERNEL}/FreeRTOS-Kernel${EBISU_TEST_KERNEL}.zip
    URL_HASH SHA256=${kernel_sha256_${EBISU_TEST_KERNEL}}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(freertos_plus_tcp
    URL https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/releases/download/${EBISU_TEST_PLUS_TCP}/FreeRTOS-Plus-TCP-${EBISU_TEST_PLUS_TCP}.zip
    URL_HASH SHA256=${plus_tcp_sha256_${EBISU_TEST_PLUS_TCP}}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

# What FreeRTOS-Kernel's and FreeRTOS-Plus-TCP's CMake ask of an application:
# FreeRTOSConfig.h's and FreeRTOSIPConfig.h's directory, the port, the heap,
# and the network interface, the board's LAN9118.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE config ${board})
set(FREERTOS_PORT GCC_ARM_CM3)
set(FREERTOS_HEAP 3)
set(FREERTOS_PLUS_TCP_NETWORK_IF MPS2_AN385 CACHE STRING "" FORCE)
FetchContent_MakeAvailable(freertos_kernel freertos_plus_tcp)

# ebisu, with its port to FreeRTOS-Plus-TCP, which its CMake builds as the
# kernel's and FreeRTOS-Plus-TCP's targets are there.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)

add_executable(freertos_tcp_test
    freertos_tcp_test.c
    ${board}/board.c
    ${board}/startup.c
    ${board}/syscalls.c)
set_target_properties(freertos_tcp_test PROPERTIES
    SUFFIX .elf
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    LINK_DEPENDS ${board}/mps2_an385.ld)
target_include_directories(freertos_tcp_test PRIVATE ${board})
target_compile_options(freertos_tcp_test PRIVATE -Wall -Werror)
target_link_libraries(freertos_tcp_test PRIVATE
    ebisu_freertos_tcp freertos_plus_tcp_network_if freertos_kernel)
# The board's start-up code starts the program, not the C library's.
target_link_options(freertos_tcp_test PRIVATE
    -nostartfiles
    -T ${board}/mps2_an385.ld
    -Wl,--gc-sections)

enable_testing()
find_program(QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME freertos_tcp
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run_tcp_test.py
        ${QEMU_SYSTEM_ARM} $<TARGET_FILE:freertos_tcp_test>)
set_tests_properties(freertos_tcp PROPERTIES TIMEOUT 360)
