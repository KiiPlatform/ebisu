# Self-contained tests: no network, no credentials. These are the ones that
# should stay fast and fully deterministic.

ebisu_add_test_suite(small_test_khc
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/khc
    SOURCES
        khc/chunk_size_line_test.cpp
        khc/header_parser_test.cpp
        khc/http_test.cpp
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

ebisu_add_test_suite(small_test_kii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kii
    SOURCES
        kii/mqtt_test.cpp
        kii/simple_test.cpp
        kii/thing_test.cpp
    LINK ${EBISU_KII_OBJS}
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/kii ${PROJECT_SOURCE_DIR}/kii)

ebisu_add_test_suite(small_test_jkii
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES
        jkii/src/jkii_escape_test.cpp
        jkii/src/jkii_primitive_test.cpp
        jkii/src/jkii_test.cpp
    LINK ${EBISU_JKII_OBJS})

# jkii again, with jkii.c compiled with a strtod() that sets no errno, for the
# primitive tests: a double out of range is told from what strtod() gives.
get_target_property(jkii_other_sources ebisu_jkii_objs SOURCES)
list(FILTER jkii_other_sources EXCLUDE REGEX "(^|/)jkii\\.c$")
add_library(ebisu_jkii_strtod_without_errno_objs OBJECT
    jkii/strtod_without_errno.c ${jkii_other_sources})
target_include_directories(ebisu_jkii_strtod_without_errno_objs
    PRIVATE $<TARGET_PROPERTY:ebisu_jkii_objs,INCLUDE_DIRECTORIES>
        ${PROJECT_SOURCE_DIR}/jkii/src
    INTERFACE $<TARGET_PROPERTY:ebisu_jkii_objs,INTERFACE_INCLUDE_DIRECTORIES>)
ebisu_configure_target(ebisu_jkii_strtod_without_errno_objs)

ebisu_add_test_suite(small_test_jkii_strtod_without_errno
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jkii
    SOURCES jkii/strtod_without_errno_test.cpp jkii/src/jkii_primitive_test.cpp
    LINK ebisu_jkii_strtod_without_errno_objs ebisu_jsmn_objs
    INCLUDES ${PROJECT_SOURCE_DIR}/jkii/libs/jsmn)

ebisu_add_test_suite(small_test_tio
    LABEL small
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tio
    SOURCES
        tio/command_parser_test.cpp
        tio/simple_test.cpp
    LINK ${EBISU_TIO_OBJS}
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/tio ${PROJECT_SOURCE_DIR}/tio)
