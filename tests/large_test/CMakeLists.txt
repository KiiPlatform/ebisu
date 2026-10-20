# Tests against a live Kii application. They need credentials, they are slow,
# and they are not fully deterministic -- hence the separate `large` label, so
# `ctest -L small` stays clean and quick.

# APP_ID and DEFAULT_SITE are stringified by tests/large_test/test_env.h.
#
# Passing credentials as -D bakes them into the binary and puts them on the
# compiler command line, where `ps` can see them. Reading them from the
# environment at run time would be better, but that is a change to the test
# sources rather than to the build.
set(large_test_defines
    _APP_ID=${EBISU_APP_ID}
    _DEFAULT_SITE=${EBISU_DEFAULT_SITE})

option(EBISU_PLAIN_HTTP "Use unencrypted HTTP in the large tests" OFF)
if(EBISU_PLAIN_HTTP)
    list(APPEND large_test_defines PLAIN_HTTP)
endif()

# The dump holds tokens and passwords: never turn it on where the output is
# kept, as in CI, whose logs are public.
option(EBISU_SOCKET_LOG "Dump socket traffic during the large tests" OFF)
if(EBISU_SOCKET_LOG)
    list(APPEND large_test_defines SOCKET_LOG)
endif()

# Shared by both large suites: the socket callbacks, with TLS and, for
# EBISU_PLAIN_HTTP, without. OpenSSL over BSD sockets, except on Windows, where
# they are the win32 sample's, over Winsock and Schannel, so that the sample's
# TLS runs against the live server too.
#
# kii/mqtt_test.cpp connects to the MQTT broker through the platform's sample
# callbacks, which time out their receives as the push task needs: the win32
# sample's, or the Linux sample's, sock_cb_linux.c.
if(WIN32)
    set(large_test_common
        ${CMAKE_CURRENT_SOURCE_DIR}/secure_socket_win32.cpp
        ${PROJECT_SOURCE_DIR}/tio/win32-sample/sock_cb_win32.c)
    set(large_test_socket_libs ws2_32 secur32 crypt32)
    set(large_test_socket_includes
        ${PROJECT_SOURCE_DIR}/tio/win32-sample
        ${PROJECT_SOURCE_DIR}/kii/include)
    # Without it, windows.h pulls in winsock.h, which clashes with winsock2.h.
    list(APPEND large_test_defines WIN32_LEAN_AND_MEAN)
else()
    find_package(OpenSSL REQUIRED)
    set(large_test_common
        ${CMAKE_CURRENT_SOURCE_DIR}/secure_socket_impl.cpp
        ${PROJECT_SOURCE_DIR}/tio/linux-sample/sock_cb_linux.c)
    set(large_test_socket_libs OpenSSL::SSL OpenSSL::Crypto)
    set(large_test_socket_includes
        ${PROJECT_SOURCE_DIR}/tio/linux-sample
        ${PROJECT_SOURCE_DIR}/kii/include)
endif()

ebisu_add_test_suite(large_test_khc
    LABEL large
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/get_test.cpp
        khc/post_error_test.cpp
        khc/post_test.cpp
        ${large_test_common}
    LINK ${EBISU_KHC_OBJS} ${large_test_socket_libs} Threads::Threads
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/khc
        ${PROJECT_SOURCE_DIR}/khc/src
        ${large_test_socket_includes}
    DEFINES ${large_test_defines})

ebisu_add_test_suite(large_test_kii
    LABEL large
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES
        kii/api_call_test.cpp
        kii/mqtt_test.cpp
        kii/object_test.cpp
        kii/push_test.cpp
        kii/server_code_test.cpp
        kii/thing_test.cpp
        kii/ti_test.cpp
        ${large_test_common}
    LINK ${EBISU_KII_OBJS} ${large_test_socket_libs} Threads::Threads
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kii
        ${PROJECT_SOURCE_DIR}/khc/include
        ${PROJECT_SOURCE_DIR}/jkii/include
        ${PROJECT_SOURCE_DIR}/jkii/libs/jsmn
        ${large_test_socket_includes}
    DEFINES ${large_test_defines})

# tio end to end: the handler and the updater against the live application,
# with a user owning the thing. The test sets the application up as its admin,
# from CLIENT_ID and CLIENT_SECRET in the environment at run time.
ebisu_add_test_suite(large_test_tio
    LABEL large
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tio
    SOURCES
        tio/tio_test.cpp
        ${large_test_common}
    LINK ${EBISU_TIO_OBJS} ${large_test_socket_libs} Threads::Threads
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kii
        ${PROJECT_SOURCE_DIR}/khc/include
        ${PROJECT_SOURCE_DIR}/jkii/include
        ${PROJECT_SOURCE_DIR}/jkii/libs/jsmn
        ${PROJECT_SOURCE_DIR}/tio/include
        ${large_test_socket_includes}
    DEFINES ${large_test_defines})
