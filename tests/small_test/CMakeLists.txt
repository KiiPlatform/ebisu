# Self-contained tests: no network, no credentials. These are the ones that
# should stay fast and fully deterministic.

ebisu_add_test_suite(small_test_khc
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/chunk_size_line_test.cpp
        khc/header_parser_test.cpp
        khc/http_date_test.cpp
        khc/http_test.cpp
        khc/request_body_test.cpp
        khc/reset_test.cpp
        khc/response_chunk_test.cpp
        khc/response_end_test.cpp
        khc/response_test.cpp
        khc/slist_test.cpp
        khc/state_trans_test.cpp
        khc/testtest.cpp
    LINK ${EBISU_KHC_OBJS}
    # khc's tests reach into its private headers.
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/khc ${PROJECT_SOURCE_DIR}/khc/src)

ebisu_add_test_suite(small_test_kii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES
        kii/clock_test.cpp
        kii/date_test.cpp
        kii/mqtt_test.cpp
        kii/push_test.cpp
        kii/request_test.cpp
        kii/session_test.cpp
        kii/simple_test.cpp
        kii/thing_test.cpp
    # session_test runs tasks on std::thread, which before glibc 2.34 needs
    # libpthread.
    LINK ${EBISU_KII_OBJS} Threads::Threads
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/kii ${PROJECT_SOURCE_DIR}/kii)

ebisu_add_test_suite(small_test_jkii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES
        jkii/src/jkii_escape_test.cpp
        jkii/src/jkii_primitive_test.cpp
        jkii/src/jkii_test.cpp
    LINK ${EBISU_JKII_OBJS})

# platform's POSIX port, which needs threads and nothing else. Linux only, as
# the test reads a thread's attributes with pthread_getattr_np().
if(TARGET ebisu_posix AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(small_test_tio_linux_sources tio/posix_test.cpp)
    set(small_test_tio_linux_libs ebisu_posix)
endif()

ebisu_add_test_suite(small_test_tio
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tio
    SOURCES
        tio/command_parser_test.cpp
        tio/session_test.cpp
        tio/simple_test.cpp
        ${small_test_tio_linux_sources}
    LINK ${EBISU_TIO_OBJS} ${small_test_tio_linux_libs}
    INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/tio
        ${PROJECT_SOURCE_DIR}/tio)
