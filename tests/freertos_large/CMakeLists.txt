# ebisu's large tests, the suites tests/large_test defines, each built as a
# firmware for the MPS2 AN385 board QEMU emulates, a Cortex-M3 with a LAN9118
# Ethernet controller, which runs the suite in a FreeRTOS task against the live
# Kii application, over platform's mbedTLS port and its FreeRTOS-Plus-TCP
# socket. CTest runs each in QEMU, whose user-mode networking takes it to the
# Internet. FreeRTOS, FreeRTOS-Plus-TCP and mbedTLS are built as the FreeRTOS
# sample builds them, and ebisu with its own CMake. See README.md.
cmake_minimum_required(VERSION 3.24)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)
set(sample ${ebisu_root}/tio/freertos-sample)
set(board ${sample}/board)

# The FreeRTOS sample's toolchain, for the same board, unless told otherwise.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${sample}/arm-none-eabi.cmake)
endif()
# mbedTLS's handshakes are slow enough under QEMU unoptimized.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

project(ebisu_freertos_large_test C CXX)

# The application, as the host's large tests have it: its ID and site, built
# in, and its admin's credentials, CLIENT_ID and CLIENT_SECRET, which the
# firmware reads from a file on the host when it starts.
set(EBISU_APP_ID "$ENV{APP_ID}" CACHE STRING "Kii application ID used by the large tests")
set(EBISU_DEFAULT_SITE "$ENV{DEFAULT_SITE}" CACHE STRING "Kii site used by the large tests")
set(EBISU_LTEST_ENV_FILE "" CACHE FILEPATH
    "A file of the host's whose lines, NAME=value, the firmware sets in its environment: CLIENT_ID, CLIENT_SECRET and CI")
if(NOT EBISU_APP_ID OR NOT EBISU_DEFAULT_SITE)
    message(FATAL_ERROR
        "Set APP_ID and DEFAULT_SITE in the environment, or -DEBISU_APP_ID=... -DEBISU_DEFAULT_SITE=...")
endif()

# For --gc-sections, which leaves out what nothing uses.
add_compile_options(-ffunction-sections -fdata-sections)

# FreeRTOS, FreeRTOS-Plus-TCP and mbedTLS, the sample's releases, checked
# against their hashes.
include(FetchContent)
FetchContent_Declare(freertos_kernel
    URL https://github.com/FreeRTOS/FreeRTOS-Kernel/releases/download/V11.3.3/FreeRTOS-KernelV11.3.3.zip
    URL_HASH SHA256=dd728be9ecfdcea4121b89ed015dff77eb0d3fa8ce2bff4b660da748b298990f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(freertos_plus_tcp
    URL https://github.com/FreeRTOS/FreeRTOS-Plus-TCP/releases/download/V4.4.2/FreeRTOS-Plus-TCP-V4.4.2.zip
    URL_HASH SHA256=bea95f97c6eecfc957e3fba411a21055ef9f26906b3268bee911a7d2756bf181
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR not-built)
FetchContent_Declare(mbedtls
    URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2
    URL_HASH SHA256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR not-built)
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE config)
set(FREERTOS_PORT GCC_ARM_CM3)
set(FREERTOS_HEAP 3)
FetchContent_MakeAvailable(freertos_kernel freertos_plus_tcp mbedtls)

# The TCP/IP stack, with the board's Ethernet driver.
set(tcp ${freertos_plus_tcp_SOURCE_DIR}/source)
set(tcp_driver ${tcp}/portable/NetworkInterface/MPS2_AN385)
file(GLOB tcp_srcs ${tcp}/*.c)
add_library(freertos_plus_tcp STATIC
    ${tcp_srcs}
    ${tcp}/portable/BufferManagement/BufferAllocation_2.c
    ${tcp_driver}/NetworkInterface.c
    ${tcp_driver}/ether_lan9118/smsc9220_eth_drv.c)
target_include_directories(freertos_plus_tcp
    SYSTEM PUBLIC ${tcp}/include ${tcp}/portable/Compiler/GCC
    PRIVATE ${tcp_driver}/ether_lan9118)
target_link_libraries(freertos_plus_tcp PUBLIC freertos_kernel)

# mbedTLS with the sample's configuration, under the name ebisu's CMake builds
# its mbedTLS port for.
set(mbedtls ${mbedtls_SOURCE_DIR})
file(GLOB mbedtls_srcs ${mbedtls}/library/*.c)
add_library(mbedtls STATIC ${mbedtls_srcs})
target_include_directories(mbedtls
    SYSTEM PUBLIC ${mbedtls}/include config
    PRIVATE ${mbedtls}/library)
target_compile_definitions(mbedtls PUBLIC
    MBEDTLS_CONFIG_FILE="${sample}/config/sample_mbedtls_config.h")
add_library(MbedTLS::mbedtls ALIAS mbedtls)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)

# The sample's CA certificates into ca_pem.c, as the sample has them.
file(STRINGS ${sample}/ca.pem ca_lines)
set(CA_PEM_LINES "")
foreach(line IN LISTS ca_lines)
    string(REPLACE "\\" "\\\\" line "${line}")
    string(REPLACE "\"" "\\\"" line "${line}")
    string(APPEND CA_PEM_LINES "    \"${line}\\n\"\n")
endforeach()
configure_file(${sample}/config/ca_pem.c.in ca_pem.c @ONLY)

# What runs a suite: the board; the sample's network, random generator and
# mbedTLS mutexes; the FreeRTOS task the suite runs in; and the C library's
# calls, over semihosting, as the small tests' firmware has them.
add_library(suite_runner OBJECT
    large_main.cpp
    ${ebisu_root}/tests/freertos_small/syscalls.c
    ${sample}/log.c
    ${sample}/mbedtls_threading.c
    ${sample}/net.c
    ${sample}/random.c
    ${board}/board.c
    ${board}/startup.c
    ${CMAKE_CURRENT_BINARY_DIR}/ca_pem.c)
# The large tests' platform on the board, ltest_board.h, with the socket
# callbacks, for tests/large_test/CMakeLists.txt to build each suite with.
set(board_libs ebisu_mbedtls ebisu_freertos_tcp freertos_plus_tcp mbedtls freertos_kernel)
add_library(ebisu_ltest_board INTERFACE)
target_sources(ebisu_ltest_board INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/secure_socket_freertos.cpp)
target_include_directories(ebisu_ltest_board INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(ebisu_ltest_board INTERFACE EBISU_LTEST_BOARD)
target_link_libraries(ebisu_ltest_board INTERFACE ${board_libs})

# As tests/CMakeLists.txt has them, for tests/large_test's suites.
add_library(ebisu_catch INTERFACE)
target_include_directories(ebisu_catch INTERFACE ${ebisu_root}/tests)

# ltest_platform.h's names too, as ltest_board.h has them, without the socket
# callbacks, which each suite has.
target_include_directories(suite_runner PRIVATE
    ${sample} ${board} ${CMAKE_CURRENT_SOURCE_DIR} ${ebisu_root}/tests/large_test)
target_compile_definitions(suite_runner PRIVATE EBISU_LTEST_BOARD)
target_compile_options(suite_runner PRIVATE -Wall -Werror)
target_link_libraries(suite_runner PUBLIC ${board_libs} ebisu_catch)
if(EBISU_LTEST_ENV_FILE)
    set_property(SOURCE large_main.cpp APPEND PROPERTY COMPILE_DEFINITIONS
        EBISU_LTEST_ENV_FILE="${EBISU_LTEST_ENV_FILE}")
endif()
set(EBISU_JKII_OBJS ebisu_jkii_objs ebisu_core_objs)
set(EBISU_KHC_OBJS ebisu_khc_objs ebisu_core_objs)
set(EBISU_KII_OBJS ebisu_kii_objs ${EBISU_KHC_OBJS} ${EBISU_JKII_OBJS})
list(REMOVE_DUPLICATES EBISU_KII_OBJS)
set(EBISU_TIO_OBJS ebisu_tio_objs ${EBISU_KII_OBJS})
# Threads, which the suites link on a host only.
add_library(Threads::Threads INTERFACE IMPORTED)

find_program(QEMU_SYSTEM_ARM qemu-system-arm REQUIRED)

# tests/large_test's, for a firmware of each suite: its sources, with Catch's
# main() renamed for the runner to call, run in QEMU in the suite's working
# directory, with QEMU's user-mode networking on the board's Ethernet
# controller.
function(ebisu_add_test_suite name)
    cmake_parse_arguments(T "" "LABEL;WORKING_DIR" "SOURCES;LINK;INCLUDES;DEFINES" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    set_target_properties(${name} PROPERTIES
        SUFFIX .elf
        LINK_DEPENDS ${board}/mps2_an385.ld)
    target_compile_features(${name} PRIVATE cxx_std_11)
    target_compile_options(${name} PRIVATE -Wall)
    # As the small tests' firmware has them.
    target_compile_definitions(${name} PRIVATE
        main=ebisu_test_main CATCH_CONFIG_NO_POSIX_SIGNALS CATCH_CONFIG_COLOUR_NONE
        ${T_DEFINES})
    target_link_libraries(${name} PRIVATE suite_runner ebisu_catch ${T_LINK})
    if(T_INCLUDES)
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
    endif()
    target_link_options(${name} PRIVATE
        -nostartfiles
        --specs=rdimon.specs
        -T ${board}/mps2_an385.ld
        -Wl,--gc-sections)
    add_test(NAME ${name}
        COMMAND ${QEMU_SYSTEM_ARM} -machine mps2-an385 -cpu cortex-m3
            -display none -monitor none -serial stdio
            -semihosting-config enable=on,target=native,arg=${name}
            -netdev user,id=net0 -net nic,model=lan9118,netdev=net0
            -kernel $<TARGET_FILE:${name}>
        WORKING_DIRECTORY ${T_WORKING_DIR})
    set_tests_properties(${name} PROPERTIES LABELS "${T_LABEL}" TIMEOUT 2400)
endfunction()

enable_testing()
# tests/large_test's paths start at ebisu's root, its project's directory.
set(PROJECT_SOURCE_DIR ${ebisu_root})
add_subdirectory(${ebisu_root}/tests/large_test large_test)
