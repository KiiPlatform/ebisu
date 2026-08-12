# Tests against a live Kii application. They need credentials, they are slow,
# and they are not fully deterministic -- hence the separate `large` label, so
# `ctest -L small` stays clean and quick.

find_package(OpenSSL REQUIRED)

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

option(EBISU_SOCKET_LOG "Dump socket traffic during the large tests" OFF)
if(EBISU_SOCKET_LOG)
    list(APPEND large_test_defines SOCKET_LOG)
endif()

# Shared by both large suites: the OpenSSL-backed socket callbacks.
set(large_test_common ${CMAKE_CURRENT_SOURCE_DIR}/secure_socket_impl.cpp)

ebisu_add_test_suite(large_test_khc
    LABEL large
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/get_test.cpp
        khc/post_error_test.cpp
        khc/post_test.cpp
        ${large_test_common}
    LINK khc OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/khc
        ${PROJECT_SOURCE_DIR}/khc/src
    DEFINES ${large_test_defines})

ebisu_add_test_suite(large_test_kii
    LABEL large
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES
        kii/api_call_test.cpp
        kii/object_test.cpp
        kii/push_test.cpp
        kii/server_code_test.cpp
        kii/thing_test.cpp
        kii/ti_test.cpp
        ${large_test_common}
    LINK kii OpenSSL::SSL OpenSSL::Crypto Threads::Threads
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kii
        ${PROJECT_SOURCE_DIR}/khc/include
        ${PROJECT_SOURCE_DIR}/jkii/include
        ${PROJECT_SOURCE_DIR}/jkii/libs/jsmn
    DEFINES ${large_test_defines})
