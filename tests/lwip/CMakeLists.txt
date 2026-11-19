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

# The releases' tags' archives, checked against their hashes.
set(lwip_sha256_1.4.1 8959cd9520a47e25a99054e2e677748591127c0b4228a5ca1552dd522490e29e)
set(lwip_sha256_2.0.2 690e1303e71f0d21ff7e773e43990d52ac2d46208b8b72c0cd0d44e011959c49)
set(lwip_sha256_2.1.3 6add930f7c0c7a8adbe3fef393ecf47ce32ed8dadd00d23042ff61db05b194ef)
set(lwip_sha256_2.2.1 ce0b7461c0ad9602c376f0bf07c5eb7253b48c7bf66f011c6bf3e2a96731c539)
if(NOT DEFINED lwip_sha256_${EBISU_TEST_LWIP})
    message(FATAL_ERROR "No such lwIP to test with: ${EBISU_TEST_LWIP}")
endif()
if(EBISU_TEST_CONFIG STREQUAL "a")
    set(config_b 0)
elseif(EBISU_TEST_CONFIG STREQUAL "b")
    set(config_b 1)
else()
    message(FATAL_ERROR "No such configuration: ${EBISU_TEST_CONFIG}")
endif()

# lwIP 1.4's tag has no _RELEASE in its name.
string(REPLACE "." "_" tag ${EBISU_TEST_LWIP})
if(EBISU_TEST_LWIP VERSION_LESS 2.0)
    set(lwip_1 1)
    set(lwip_tag STABLE-${tag})
else()
    set(lwip_1 0)
    set(lwip_tag STABLE-${tag}_RELEASE)
endif()
include(FetchContent)
# lwIP's own CMakeLists.txt builds its examples, which this doesn't: from a
# directory without one, its sources are only fetched.
FetchContent_Declare(lwip
    URL https://github.com/lwip-tcpip/lwip/archive/refs/tags/${lwip_tag}.tar.gz
    URL_HASH SHA256=${lwip_sha256_${EBISU_TEST_LWIP}}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR no-cmake-here)
FetchContent_MakeAvailable(lwip)

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

if(lwip_1)
    # 1.4 has a list of its sources for make only too, of other files: IPv4
    # alone, and its timers in timers.c. Its dns_setserver() ignores NULL and
    # IP_ADDR_ANY, so that a server once set can't be unset, as the tests do
    # between their sections: its dns.c with 2.0's, which takes NULL for none.
    set(src ${LWIP_DIR}/src)
    file(READ ${src}/core/dns.c dns_c)
    set(setter_1 "  if ((numdns < DNS_MAX_SERVERS) && (dns_pcb != NULL) &&
      (dnsserver != NULL) && !ip_addr_isany(dnsserver)) {
    dns_servers[numdns] = (*dnsserver);
  }")
    set(setter_2 "  if ((numdns < DNS_MAX_SERVERS) && (dns_pcb != NULL)) {
    if (dnsserver != NULL) {
      dns_servers[numdns] = (*dnsserver);
    } else {
      ip_addr_set_any(&dns_servers[numdns]);
    }
  }")
    string(FIND "${dns_c}" "${setter_1}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${src}/core/dns.c has no dns_setserver() as 1.4.1's")
    endif()
    string(REPLACE "${setter_1}" "${setter_2}" dns_c "${dns_c}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lwip/dns.c "${dns_c}")
    add_library(lwipcore STATIC
        ${src}/core/def.c ${src}/core/dhcp.c ${CMAKE_CURRENT_BINARY_DIR}/lwip/dns.c ${src}/core/init.c
        ${src}/core/mem.c ${src}/core/memp.c ${src}/core/netif.c ${src}/core/pbuf.c
        ${src}/core/raw.c ${src}/core/stats.c ${src}/core/sys.c ${src}/core/tcp.c
        ${src}/core/tcp_in.c ${src}/core/tcp_out.c ${src}/core/timers.c ${src}/core/udp.c
        ${src}/core/ipv4/autoip.c ${src}/core/ipv4/icmp.c ${src}/core/ipv4/igmp.c
        ${src}/core/ipv4/inet.c ${src}/core/ipv4/inet_chksum.c ${src}/core/ipv4/ip.c
        ${src}/core/ipv4/ip_addr.c ${src}/core/ipv4/ip_frag.c
        ${src}/api/api_lib.c ${src}/api/api_msg.c ${src}/api/err.c ${src}/api/netbuf.c
        ${src}/api/netdb.c ${src}/api/netifapi.c ${src}/api/sockets.c ${src}/api/tcpip.c
        ${src}/netif/etharp.c)
    target_include_directories(lwipcore PRIVATE ${LWIP_INCLUDE_DIRS})
    target_compile_definitions(lwipcore PRIVATE ${LWIP_DEFINITIONS})
elseif(EXISTS ${LWIP_DIR}/src/Filelists.cmake)
    # From 2.1 on, lwIP's core target, lwipcore.
    include(${LWIP_DIR}/src/Filelists.cmake)
else()
    # 2.0 has a list of its sources for make only: lwipcore made of the same,
    # as an application's build would.
    set(src ${LWIP_DIR}/src)
    add_library(lwipcore STATIC
        ${src}/core/init.c ${src}/core/def.c ${src}/core/dns.c ${src}/core/inet_chksum.c
        ${src}/core/ip.c ${src}/core/mem.c ${src}/core/memp.c ${src}/core/netif.c
        ${src}/core/pbuf.c ${src}/core/raw.c ${src}/core/stats.c ${src}/core/sys.c
        ${src}/core/tcp.c ${src}/core/tcp_in.c ${src}/core/tcp_out.c ${src}/core/timeouts.c
        ${src}/core/udp.c
        ${src}/core/ipv4/autoip.c ${src}/core/ipv4/dhcp.c ${src}/core/ipv4/etharp.c
        ${src}/core/ipv4/icmp.c ${src}/core/ipv4/igmp.c ${src}/core/ipv4/ip4_frag.c
        ${src}/core/ipv4/ip4.c ${src}/core/ipv4/ip4_addr.c
        ${src}/core/ipv6/dhcp6.c ${src}/core/ipv6/ethip6.c ${src}/core/ipv6/icmp6.c
        ${src}/core/ipv6/inet6.c ${src}/core/ipv6/ip6.c ${src}/core/ipv6/ip6_addr.c
        ${src}/core/ipv6/ip6_frag.c ${src}/core/ipv6/mld6.c ${src}/core/ipv6/nd6.c
        ${src}/api/api_lib.c ${src}/api/api_msg.c ${src}/api/err.c ${src}/api/netbuf.c
        ${src}/api/netdb.c ${src}/api/netifapi.c ${src}/api/sockets.c ${src}/api/tcpip.c
        ${src}/netif/ethernet.c)
    target_include_directories(lwipcore PRIVATE ${LWIP_INCLUDE_DIRS})
    target_compile_definitions(lwipcore PRIVATE ${LWIP_DEFINITIONS})
endif()

# The port: semaphores, mutexes, mailboxes and threads over POSIX threads.
find_package(Threads REQUIRED)
add_library(lwip_test_port STATIC port/sys_arch.c)
target_include_directories(lwip_test_port PRIVATE ${LWIP_INCLUDE_DIRS})
target_compile_definitions(lwip_test_port PRIVATE ${LWIP_DEFINITIONS})
target_link_libraries(lwip_test_port PUBLIC Threads::Threads)

# ebisu, with its lwIP socket, which its CMake builds as lwipcore is there.
set(BUILD_SHARED_LIBS OFF)
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
