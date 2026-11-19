# Self-contained tests: no network, no credentials. These are the ones that
# should stay fast and fully deterministic.

ebisu_add_test_suite(small_test_khc
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/chunk_size_line_test.cpp
        khc/deadline_test.cpp
        khc/sock_error_test.cpp
        khc/header_parser_test.cpp
        khc/http_date_test.cpp
        khc/http_test.cpp
        khc/logger_test.cpp
        khc/request_body_test.cpp
        khc/reset_test.cpp
        khc/response_chunk_test.cpp
        khc/response_end_test.cpp
        khc/response_parse_test.cpp
        khc/response_test.cpp
        khc/slist_test.cpp
        khc/state_trans_test.cpp
        khc/testtest.cpp
    LINK ${EBISU_KHC_OBJS}
    # khc's tests reach into its private headers.
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/khc ${PROJECT_SOURCE_DIR}/khc/src)

# ebisu_objs_defining(<target> <objs> <definition>)
#
# Builds <target>'s sources again as <objs>, an object library, with its
# include directories and definitions and <definition> besides, as an option
# that defines <definition> would build them, so that each build runs a
# library's tests both ways.
function(ebisu_objs_defining target objs definition)
    get_target_property(dir ${target} SOURCE_DIR)
    get_target_property(target_sources ${target} SOURCES)
    set(sources "")
    foreach(source IN LISTS target_sources)
        if(NOT IS_ABSOLUTE "${source}")
            set(source "${dir}/${source}")
        endif()
        list(APPEND sources "${source}")
    endforeach()
    add_library(${objs} OBJECT ${sources})
    target_include_directories(${objs}
        PRIVATE $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>
        INTERFACE $<TARGET_PROPERTY:${target},INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(${objs}
        PRIVATE $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS> ${definition})
    ebisu_configure_target(${objs})
endfunction()

set(kii_test_sources
    kii/clock_test.cpp
    kii/date_test.cpp
    kii/mqtt_test.cpp
    kii/push_test.cpp
    kii/request_test.cpp
    kii/secrets_test.cpp
    kii/server_code_test.cpp
    kii/simple_test.cpp
    kii/thing_test.cpp)
# Sessions are tested where the libraries have them, -DEBISU_SESSION=OFF
# leaving them out.
set(kii_session_test "")
if(EBISU_SESSION)
    set(kii_session_test kii/session_test.cpp)
endif()

ebisu_add_test_suite(small_test_kii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES ${kii_test_sources} ${kii_session_test}
    # session_test runs tasks on std::thread, which before glibc 2.34 needs
    # libpthread.
    LINK ${EBISU_KII_OBJS} Threads::Threads
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/kii ${PROJECT_SOURCE_DIR}/kii)

# kii again, without sessions, as -DEBISU_SESSION=OFF builds it: its tests but
# the sessions', and that no session can be attached.
ebisu_objs_defining(ebisu_kii_objs ebisu_kii_no_session_objs EBISU_NO_SESSION)
target_link_libraries(ebisu_kii_no_session_objs
    PRIVATE ebisu_khc_objs ebisu_core_objs ebisu_jkii_objs)

ebisu_add_test_suite(small_test_kii_no_session
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES ${kii_test_sources} kii/no_session_test.cpp
    LINK ebisu_kii_no_session_objs ${EBISU_KHC_OBJS} ${EBISU_JKII_OBJS} Threads::Threads
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/kii ${PROJECT_SOURCE_DIR}/kii
    DEFINES EBISU_NO_SESSION)

set(jkii_test_sources
    jkii/src/jkii_double_test.cpp
    jkii/src/jkii_escape_test.cpp
    jkii/src/jkii_find_test.cpp
    jkii/src/jkii_lines_test.cpp
    jkii/src/jkii_primitive_test.cpp
    jkii/src/jkii_read_test.cpp
    jkii/src/jkii_test.cpp
    jkii/src/jkii_unescape_test.cpp
    jkii/src/jkii_writer_test.cpp
    # An application's own coreJSON, beside jkii's.
    jkii/src/app_core_json_test.cpp
    ${PROJECT_SOURCE_DIR}/jkii/libs/coreJSON/core_json.c)

ebisu_add_test_suite(small_test_jkii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES ${jkii_test_sources}
    LINK ${EBISU_JKII_OBJS}
    INCLUDES ${PROJECT_SOURCE_DIR}/jkii/libs/coreJSON)

# jkii again, reading doubles without strtod(), as -DEBISU_STRTOD=OFF builds it.
ebisu_objs_defining(ebisu_jkii_objs ebisu_jkii_no_strtod_objs EBISU_NO_STRTOD)
target_link_libraries(ebisu_jkii_no_strtod_objs PUBLIC ebisu_core_objs)

ebisu_add_test_suite(small_test_jkii_no_strtod
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES ${jkii_test_sources}
    LINK ebisu_jkii_no_strtod_objs ebisu_core_objs
    INCLUDES ${PROJECT_SOURCE_DIR}/jkii/libs/coreJSON
    DEFINES EBISU_NO_STRTOD)

# jkii again, with jkii.c compiled with a strtod() that sets no errno, for the
# double tests: a double out of range is told from what strtod() gives.
if(EBISU_STRTOD)
    get_target_property(jkii_other_sources ebisu_jkii_objs SOURCES)
    list(FILTER jkii_other_sources EXCLUDE REGEX "(^|/)jkii\\.c$")
    add_library(ebisu_jkii_strtod_without_errno_objs OBJECT
        jkii/strtod_without_errno.c ${jkii_other_sources})
    target_include_directories(ebisu_jkii_strtod_without_errno_objs
        PRIVATE $<TARGET_PROPERTY:ebisu_jkii_objs,INCLUDE_DIRECTORIES>
            ${PROJECT_SOURCE_DIR}/jkii/src
        INTERFACE $<TARGET_PROPERTY:ebisu_jkii_objs,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(ebisu_jkii_strtod_without_errno_objs
        PRIVATE $<TARGET_PROPERTY:ebisu_jkii_objs,COMPILE_DEFINITIONS>)
    target_link_libraries(ebisu_jkii_strtod_without_errno_objs PUBLIC ebisu_core_objs)
    ebisu_configure_target(ebisu_jkii_strtod_without_errno_objs)

    ebisu_add_test_suite(small_test_jkii_strtod_without_errno
        LABEL small
        WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
        SOURCES jkii/strtod_without_errno_test.cpp jkii/src/jkii_double_test.cpp
        LINK ebisu_jkii_strtod_without_errno_objs ebisu_core_objs)
endif()

ebisu_add_test_suite(small_test_core
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core
    SOURCES
        core/clock_test.cpp
        core/log_test.cpp
        core/mem_test.cpp
        core/str_test.cpp
        core/system_test.cpp
        core/time_test.cpp
    # Threads for the loggers' tests that log from several tasks at once.
    LINK ebisu_core_objs Threads::Threads)

# platform's loggers, which write to a file in the build tree, and send to a
# server of the test's own on the loopback.
if(TARGET ebisu_loggers)
    set(small_test_loggers_sources
        loggers/file_logger_test.cpp
        loggers/stdio_logger_test.cpp
        loggers/syslog_udp_logger_test.cpp)
    if(UNIX)
        list(APPEND small_test_loggers_sources loggers/syslog_logger_test.cpp)
    endif()
    ebisu_add_test_suite(small_test_loggers
        LABEL small
        WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
        SOURCES ${small_test_loggers_sources}
        # Core's objects, as the other suites link them, so that a Windows
        # build's test doesn't need its DLL.
        LINK ebisu_loggers ebisu_core_objs Threads::Threads)
endif()

# platform's DNS probe, over the name servers of the POSIX port, or on Windows
# the Win32 port's, which ask servers of the test's own on the loopback, and
# over the test's own.
if(TARGET ebisu_posix)
    set(small_test_dns_probe_port ebisu_posix)
    set(small_test_dns_probe_port_src ${PROJECT_SOURCE_DIR}/platform/posix/src)
elseif(TARGET ebisu_win32)
    set(small_test_dns_probe_port ebisu_win32)
    set(small_test_dns_probe_port_src ${PROJECT_SOURCE_DIR}/platform/win32/src)
endif()
if(small_test_dns_probe_port)
    ebisu_add_test_suite(small_test_dns_probe
        LABEL small
        WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
        SOURCES dns_probe/dns_probe_test.cpp dns_probe/dns_test.cpp
        LINK ebisu_dns_probe ${small_test_dns_probe_port} ebisu_core_objs Threads::Threads
        INCLUDES
            ${PROJECT_SOURCE_DIR}/platform/dns_probe/src
            ${small_test_dns_probe_port_src})
endif()

# platform's sockets of plain TCP, the POSIX port's, or on Windows the Win32
# port's, against servers of the test's own on the loopback.
if(TARGET ebisu_posix)
    set(small_test_tcp_socket_port ebisu_posix)
elseif(TARGET ebisu_win32)
    set(small_test_tcp_socket_port ebisu_win32)
endif()
if(small_test_tcp_socket_port)
    ebisu_add_test_suite(small_test_tcp_socket
        LABEL small
        WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
        SOURCES tcp_socket/tcp_socket_test.cpp
        # Core's objects, as the other suites link them, so that a Windows
        # build's test doesn't need its DLL.
        LINK ${small_test_tcp_socket_port} ebisu_core_objs Threads::Threads)
endif()

# platform's system, the POSIX port's, or on Windows the Win32 port's: the locks
# it makes, and the inline calls over its operations.
if(small_test_tcp_socket_port)
    ebisu_add_test_suite(small_test_system
        LABEL small
        WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
        SOURCES system/system_test.cpp
        LINK ${small_test_tcp_socket_port} ebisu_core_objs Threads::Threads)
endif()

# platform's mbedTLS port, over the POSIX port's socket, against TLS servers of
# the test's own on the loopback, made with OpenSSL, where mbedTLS, the POSIX
# port and OpenSSL all are.
if(TARGET ebisu_mbedtls AND TARGET ebisu_posix)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        ebisu_add_test_suite(small_test_mbedtls
            LABEL small
            WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
            SOURCES mbedtls/mbedtls_socket_test.cpp
            LINK ebisu_mbedtls ebisu_posix ebisu_core_objs OpenSSL::SSL OpenSSL::Crypto
                Threads::Threads)
    endif()
endif()

# platform's PolarSSL port, as the mbedTLS port's: experimental.
if(TARGET ebisu_polarssl AND TARGET ebisu_posix)
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        ebisu_add_test_suite(small_test_polarssl
            LABEL small
            WORKING_DIR ${CMAKE_CURRENT_BINARY_DIR}
            SOURCES polarssl/polarssl_socket_test.cpp
            LINK ebisu_polarssl ebisu_posix ebisu_core_objs OpenSSL::SSL OpenSSL::Crypto
                Threads::Threads)
    endif()
endif()

# platform's POSIX port, which needs threads and nothing else. Linux only, as
# the test reads a thread's attributes with pthread_getattr_np().
if(TARGET ebisu_posix AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(small_test_tio_linux_sources tio/posix_test.cpp)
    set(small_test_tio_linux_libs ebisu_posix)
endif()

set(tio_session_test "")
if(EBISU_SESSION)
    set(tio_session_test tio/session_test.cpp)
endif()

ebisu_add_test_suite(small_test_tio
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tio
    SOURCES
        tio/command_parser_test.cpp
        ${tio_session_test}
        tio/simple_test.cpp
        ${small_test_tio_linux_sources}
    LINK ${EBISU_TIO_OBJS} ${small_test_tio_linux_libs}
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/tio
        ${PROJECT_SOURCE_DIR}/tio)
