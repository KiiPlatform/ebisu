# tio on FreeRTOS, with FreeRTOS-Plus-TCP and mbedTLS, for the MPS2 AN385
# board QEMU emulates: a Cortex-M3 with a LAN9118 Ethernet controller. See
# README.md.
cmake_minimum_required(VERSION 3.24)

# The sample is for that board, so for this toolchain unless told otherwise.
if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/arm-none-eabi.cmake)
endif()
# mbedTLS's handshakes are slow enough under QEMU unoptimized.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

project(ebisu_freertos_sample C)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

# The sample's settings, as menuconfig has the ESP32 sample's.
set(EBISU_SAMPLE_APP_ID "fj9xy2fsp0ld" CACHE STRING
    "The ID of the Kii application the thing belongs to")
set(EBISU_SAMPLE_APP_HOST "api-jp.kii.com" CACHE STRING
    "The application's site")
set(EBISU_SAMPLE_VENDOR_THING_ID "" CACHE STRING
    "The vendor thing ID the thing onboards with, created if it does not exist")
set(EBISU_SAMPLE_THING_PASSWORD "" CACHE STRING
    "The thing's password")
set(EBISU_SAMPLE_THING_TYPE "" CACHE STRING
    "The thing type given when onboarding, unless empty")
set(EBISU_SAMPLE_FIRMWARE_VERSION "" CACHE STRING
    "The firmware version given when onboarding, unless empty")
set(EBISU_SAMPLE_STATE "{\"AirconAlias\":{\"PresetTemperature\":20,\"RoomTemperature\":24}}"
    CACHE STRING "The state the thing uploads, as JSON: its traits' states by alias")
set(EBISU_SAMPLE_UPDATE_PERIOD 60 CACHE STRING
    "Seconds between state uploads")
set(EBISU_SAMPLE_CLOCK_FLOOR 0 CACHE STRING
    "A time, in seconds since the epoch, the time is known not to be earlier than, if later than when ebisu's source was committed; 0 for none")
set(EBISU_SAMPLE_CA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/ca.pem CACHE FILEPATH
    "The CA certificates the server's certificate has to chain to, PEM")

foreach(setting EBISU_SAMPLE_UPDATE_PERIOD EBISU_SAMPLE_CLOCK_FLOOR)
    if(NOT ${setting} MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${setting} has to be a number: ${${setting}}")
    endif()
endforeach()

# Into sample_config.h, the strings as C string literals.
foreach(setting APP_ID APP_HOST VENDOR_THING_ID THING_PASSWORD THING_TYPE
        FIRMWARE_VERSION STATE)
    string(REPLACE "\\" "\\\\" value "${EBISU_SAMPLE_${setting}}")
    string(REPLACE "\"" "\\\"" value "${value}")
    set(${setting}_LITERAL "\"${value}\"")
endforeach()
configure_file(config/sample_config.h.in sample_config.h @ONLY)

# The CA certificates into ca_pem.c, a line of the file to a line of the string.
file(STRINGS ${EBISU_SAMPLE_CA_FILE} ca_lines)
set(CA_PEM_LINES "")
foreach(line IN LISTS ca_lines)
    string(REPLACE "\\" "\\\\" line "${line}")
    string(REPLACE "\"" "\\\"" line "${line}")
    string(APPEND CA_PEM_LINES "    \"${line}\\n\"\n")
endforeach()
configure_file(config/ca_pem.c.in ca_pem.c @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${EBISU_SAMPLE_CA_FILE})

# For --gc-sections, which leaves out what nothing uses.
add_compile_options(-ffunction-sections -fdata-sections)

# FreeRTOS, FreeRTOS-Plus-TCP and mbedTLS, from their releases, checked against
# the hashes of these. FreeRTOS is built with its own CMake, as ebisu's port to
# it needs. The others' own CMake builds are not used: SOURCE_SUBDIR names a
# directory neither has, so they are only downloaded.
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
# The kernel, as its CMake asks: its configuration, config/FreeRTOSConfig.h, the
# Cortex-M3 port, and heap_3: pvPortMalloc() is the C library's malloc(), which
# mbedTLS allocates with too. ebisu is given buffers and a pool for its headers
# instead, in main.c.
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

# Configured by config/sample_mbedtls_config.h, with config/threading_alt.h.
# Under the name mbedTLS's own CMake gives its library, MbedTLS::mbedtls, by
# which ebisu's CMake finds it, and builds its mbedTLS port.
set(mbedtls ${mbedtls_SOURCE_DIR})
file(GLOB mbedtls_srcs ${mbedtls}/library/*.c)
add_library(mbedtls STATIC ${mbedtls_srcs})
target_include_directories(mbedtls
    SYSTEM PUBLIC ${mbedtls}/include config
    PRIVATE ${mbedtls}/library)
target_compile_definitions(mbedtls PUBLIC
    MBEDTLS_CONFIG_FILE="sample_mbedtls_config.h")
add_library(MbedTLS::mbedtls ALIAS mbedtls)

# ebisu, with its own CMake: tio and what it needs, and the ports to FreeRTOS,
# FreeRTOS-Plus-TCP and mbedTLS, which it builds as their targets are there. Its version header comes
# from the repository's git history.
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)

add_executable(ebisu_freertos_sample
    main.c
    log.c
    net.c
    mbedtls_threading.c
    random.c
    board/board.c
    board/startup.c
    board/syscalls.c
    ${CMAKE_CURRENT_BINARY_DIR}/ca_pem.c)
set_target_properties(ebisu_freertos_sample PROPERTIES
    SUFFIX .elf
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
    LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/board/mps2_an385.ld)
target_include_directories(ebisu_freertos_sample PRIVATE
    .
    board
    ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(ebisu_freertos_sample PRIVATE -Wall -Werror)
target_link_libraries(ebisu_freertos_sample PRIVATE
    tio ebisu_freertos ebisu_freertos_tcp ebisu_mbedtls freertos_plus_tcp mbedtls)
# board/startup.c starts the program, not the C library's.
target_link_options(ebisu_freertos_sample PRIVATE
    -nostartfiles
    -T ${CMAKE_CURRENT_SOURCE_DIR}/board/mps2_an385.ld
    -Wl,--gc-sections
    -Wl,-Map=$<TARGET_FILE_DIR:ebisu_freertos_sample>/ebisu_freertos_sample.map)
