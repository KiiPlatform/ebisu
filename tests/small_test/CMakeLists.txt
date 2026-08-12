# Self-contained tests: no network, no credentials. These are the ones that
# should stay fast and fully deterministic.

ebisu_add_test_suite(small_test_khc
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/header_parser_test.cpp
        khc/http_test.cpp
        khc/response_chunk_test.cpp
        khc/response_test.cpp
        khc/slist_test.cpp
        khc/state_trans_test.cpp
        khc/testtest.cpp
    LINK khc
    # khc's tests reach into its private headers.
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/khc ${PROJECT_SOURCE_DIR}/khc/src)

ebisu_add_test_suite(small_test_kii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES
        kii/mqtt_test.cpp
        kii/simple_test.cpp
    LINK kii
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/kii ${PROJECT_SOURCE_DIR}/kii)

ebisu_add_test_suite(small_test_jkii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES
        jkii/src/jkii_primitive_test.cpp
        jkii/src/jkii_test.cpp
    LINK jkii)

ebisu_add_test_suite(small_test_tio
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tio
    SOURCES
        tio/command_parser_test.cpp
        tio/simple_test.cpp
    LINK tio
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/tio ${PROJECT_SOURCE_DIR}/tio)
