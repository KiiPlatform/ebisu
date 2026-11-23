# The lwIP socket's tests, on the host: lwIP built from its release, with the
# tests' own port to POSIX threads and options, port/, and ebisu with its own
# CMake, which builds its lwIP socket as lwIP's core target is there, as an
# application builds them. See README.md.
cmake_minimum_required(VERSION 3.24)

project(ebisu_lwip_test C CXX)

get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../.. ABSOLUTE)

set(EBISU_TEST_LWIP 2.2.1 CACHE STRING
    "lwIP: 1.4.1, the oldest the socket supports, 2.0.2, 2.1.3, or 2.2.1, the latest")
set(EBISU_TEST_CONFIG a CACHE STRING
    "What lwIP is configured with: a or b, as port/lwipopts.h says")

include(${CMAKE_CURRENT_SOURCE_DIR}/lwip.cmake)
if(NOT DEFINED ebisu_lwip_sha256_${EBISU_TEST_LWIP})
    message(FATAL_ERROR "No such lwIP to test with: ${EBISU_TEST_LWIP}")
endif()
if(EBISU_TEST_CONFIG STREQUAL "a")
    set(config_b 0)
elseif(EBISU_TEST_CONFIG STREQUAL "b")
    set(config_b 1)
else()
    message(FATAL_ERROR "No such configuration: ${EBISU_TEST_CONFIG}")
endif()

# The release, from lwip.cmake, which builds lwipcore below.
ebisu_test_lwip(${EBISU_TEST_LWIP})
set(lwip_1 ${ebisu_test_lwip_1})
# What lwIP's CMake asks of an application, and ebisu's takes too: where
# lwIP is, and its include directories and definitions, the application's
# lwipopts.h and its port's arch/ among them, and, in lwIP 1.4, IPv4's
# headers, in a directory of their own.
set(LWIP_DIR ${lwip_SOURCE_DIR})
set(LWIP_INCLUDE_DIRS ${LWIP_DIR}/src/include ${CMAKE_CURRENT_SOURCE_DIR}/port)
if(lwip_1)
    list(APPEND LWIP_INCLUDE_DIRS ${LWIP_DIR}/src/include/ipv4)
endif()
set(LWIP_DEFINITIONS TEST_CONFIG_B=${config_b} TEST_LWIP_1=${lwip_1})
# lwIP 1.4's dns_setserver() as 2.0's, taking NULL for no server, as the tests
# unset the servers between their sections.
ebisu_test_lwipcore(DNS_SETSERVER_NULL)
# The port: semaphores, mutexes, mailboxes and threads over POSIX threads.
find_package(Threads REQUIRED)
add_library(lwip_test_port STATIC port/sys_arch.c)
target_include_directories(lwip_test_port PRIVATE ${LWIP_INCLUDE_DIRS})
target_compile_definitions(lwip_test_port PRIVATE ${LWIP_DEFINITIONS})
target_link_libraries(lwip_test_port PUBLIC Threads::Threads)

# ebisu, with its lwIP socket, which its CMake builds as lwipcore is there.
set(BUILD_SHARED_LIBS OFF)
# Configuration b with the address the socket keeps for TLS sessions to
# resume, configuration a without, as by default.
set(EBISU_TLS_RESUME ${config_b} CACHE BOOL "" FORCE)
add_subdirectory(${ebisu_root} ebisu EXCLUDE_FROM_ALL)
if(NOT TARGET ebisu_lwip)
    message(FATAL_ERROR "ebisu's CMake built no ebisu_lwip with lwipcore there")
endif()

# ebisu_lwip.h on its own, as an application includes it, under the
# libraries' -std=c99 -pedantic-errors -Wall -Werror, as tests/header_check
# compiles the headers of the libraries ebisu's own build makes.
set(header ebisu_lwip.h)
string(MAKE_C_IDENTIFIER ${header} header_id)
configure_file(${ebisu_root}/tests/header_check/header_check.c.in
    ${CMAKE_CURRENT_BINARY_DIR}/header_check.c @ONLY)
add_library(lwip_header_check OBJECT ${CMAKE_CURRENT_BINARY_DIR}/header_check.c)
target_link_libraries(lwip_header_check PRIVATE ebisu_lwip)
ebisu_configure_target(lwip_header_check)

add_executable(lwip_test lwip_test_main.cpp lwip_tcp_test.cpp lwip_dns_test.cpp)
target_compile_features(lwip_test PRIVATE cxx_std_11)
target_compile_options(lwip_test PRIVATE -Wall)
target_include_directories(lwip_test PRIVATE ${ebisu_root}/tests)
# lwIP's, whose 1.4 headers, written for C, C++11 warns of.
target_include_directories(lwip_test SYSTEM PRIVATE ${LWIP_INCLUDE_DIRS})
target_compile_definitions(lwip_test PRIVATE ${LWIP_DEFINITIONS})
target_link_libraries(lwip_test PRIVATE ebisu_lwip ebisu_dns_probe lwipcore lwip_test_port)

enable_testing()
add_test(NAME lwip COMMAND lwip_test)
set_tests_properties(lwip PROPERTIES TIMEOUT 120)
