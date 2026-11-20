# lwIP for the tests, as an application builds it: the release a test names,
# checked against its hash, as the target lwipcore, which ebisu's CMake builds
# its lwIP socket for. In two steps, as the caller's include directories need
# lwIP's own:
#
# - ebisu_test_lwip(<version>) fetches it, and sets LWIP_DIR, where lwIP is,
#   and ebisu_test_lwip_1, 1 for lwIP 1.4, 0 for 2.0 on;
# - ebisu_test_lwipcore([DNS_SETSERVER_NULL]) builds lwipcore with the
#   caller's LWIP_INCLUDE_DIRS and LWIP_DEFINITIONS, as lwIP's CMake asks of an
#   application, and ebisu's takes too: lwIP's include directories, the
#   application's lwipopts.h and its port's arch/ among them, and, in lwIP
#   1.4, IPv4's headers, in a directory of their own, LWIP_DIR/src/include/ipv4.
#   With DNS_SETSERVER_NULL, lwIP 1.4's dns_setserver() takes NULL for no
#   server, as 2.0's does.
#
# Macros, so that the caller has what they set.

# The releases' tags' archives, checked against their hashes.
set(ebisu_lwip_sha256_1.4.1 8959cd9520a47e25a99054e2e677748591127c0b4228a5ca1552dd522490e29e)
set(ebisu_lwip_sha256_2.0.2 690e1303e71f0d21ff7e773e43990d52ac2d46208b8b72c0cd0d44e011959c49)
set(ebisu_lwip_sha256_2.1.3 6add930f7c0c7a8adbe3fef393ecf47ce32ed8dadd00d23042ff61db05b194ef)
set(ebisu_lwip_sha256_2.2.1 ce0b7461c0ad9602c376f0bf07c5eb7253b48c7bf66f011c6bf3e2a96731c539)

macro(ebisu_test_lwip version)
    if(NOT DEFINED ebisu_lwip_sha256_${version})
        message(FATAL_ERROR "No such lwIP to test with: ${version}")
    endif()
    # lwIP 1.4's tag has no _RELEASE in its name.
    string(REPLACE "." "_" ebisu_lwip_tag ${version})
    if(${version} VERSION_LESS 2.0)
        set(ebisu_test_lwip_1 1)
        set(ebisu_lwip_tag STABLE-${ebisu_lwip_tag})
    else()
        set(ebisu_test_lwip_1 0)
        set(ebisu_lwip_tag STABLE-${ebisu_lwip_tag}_RELEASE)
    endif()
    include(FetchContent)
    # lwIP's own CMakeLists.txt builds its examples, which this doesn't: from a
    # directory without one, its sources are only fetched.
    FetchContent_Declare(lwip
        URL https://github.com/lwip-tcpip/lwip/archive/refs/tags/${ebisu_lwip_tag}.tar.gz
        URL_HASH SHA256=${ebisu_lwip_sha256_${version}}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR no-cmake-here)
    FetchContent_MakeAvailable(lwip)
    set(LWIP_DIR ${lwip_SOURCE_DIR})
endmacro()

macro(ebisu_test_lwipcore)
    cmake_parse_arguments(ebisu_lwipcore "DNS_SETSERVER_NULL" "" "" ${ARGN})
    set(ebisu_lwip_src ${LWIP_DIR}/src)
    if(ebisu_test_lwip_1)
        # 1.4 has a list of its sources for make only too, of other files:
        # IPv4 alone, and its timers in timers.c.
        set(ebisu_lwip_dns_c ${ebisu_lwip_src}/core/dns.c)
        if(ebisu_lwipcore_DNS_SETSERVER_NULL)
            # Its dns_setserver() ignores NULL and IP_ADDR_ANY, so that a
            # server once set can't be unset: its dns.c with 2.0's, which
            # takes NULL for none.
            file(READ ${ebisu_lwip_src}/core/dns.c ebisu_dns_c)
            set(ebisu_setter_1 "  if ((numdns < DNS_MAX_SERVERS) && (dns_pcb != NULL) &&
      (dnsserver != NULL) && !ip_addr_isany(dnsserver)) {
    dns_servers[numdns] = (*dnsserver);
  }")
            set(ebisu_setter_2 "  if ((numdns < DNS_MAX_SERVERS) && (dns_pcb != NULL)) {
    if (dnsserver != NULL) {
      dns_servers[numdns] = (*dnsserver);
    } else {
      ip_addr_set_any(&dns_servers[numdns]);
    }
  }")
            string(FIND "${ebisu_dns_c}" "${ebisu_setter_1}" ebisu_at)
            if(ebisu_at EQUAL -1)
                message(FATAL_ERROR
                    "${ebisu_lwip_src}/core/dns.c has no dns_setserver() as 1.4.1's")
            endif()
            string(REPLACE "${ebisu_setter_1}" "${ebisu_setter_2}" ebisu_dns_c "${ebisu_dns_c}")
            set(ebisu_lwip_dns_c ${CMAKE_CURRENT_BINARY_DIR}/lwip/dns.c)
            file(WRITE ${ebisu_lwip_dns_c} "${ebisu_dns_c}")
        endif()
        set(src ${ebisu_lwip_src})
        add_library(lwipcore STATIC
            ${src}/core/def.c ${src}/core/dhcp.c ${ebisu_lwip_dns_c} ${src}/core/init.c
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
        # 2.0 has a list of its sources for make only: lwipcore made of the
        # same, as an application's build would.
        set(src ${ebisu_lwip_src})
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
    unset(src)
endmacro()
