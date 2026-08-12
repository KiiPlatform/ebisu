# The tests are C++ (Catch2); the libraries under test are C. CXX is enabled
# here rather than in the root project() call so that consumers embedding ebisu
# are never asked for a C++ compiler.
enable_language(CXX)

find_package(Threads REQUIRED)

# Catch2 v2 single header, vendored at tests/catch.hpp, alongside picojson.h and
# the shared test_callbacks.h.
add_library(ebisu_catch INTERFACE)
target_include_directories(ebisu_catch INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

option(EBISU_TEST_JUNIT "Have each test suite write a JUnit report" OFF)
set(EBISU_JUNIT_DIR ${CMAKE_BINARY_DIR}/test-results/junit)
if(EBISU_TEST_JUNIT)
    # Catch does not create the directory itself.
    file(MAKE_DIRECTORY ${EBISU_JUNIT_DIR})
endif()

# Declares one CTest test per Catch2 binary.
#
# LABEL is what `ctest -L small` and `ctest -L large` select on: the small tests
# are self-contained, the large ones talk to a live Kii application.
#
# WORKING_DIR matters: the khc suite opens fixtures as "./data/resp-login.txt",
# relative to the working directory. Each per-directory Makefile happened to run
# its binary from its own source directory, so this went unnoticed; running from
# the build tree instead silently drops those 238 assertions.
function(ebisu_add_test_suite name)
    cmake_parse_arguments(T "" "LABEL;WORKING_DIR" "SOURCES;LINK;INCLUDES;DEFINES" ${ARGN})

    add_executable(${name} ${T_SOURCES})
    target_compile_features(${name} PRIVATE cxx_std_11)
    target_link_libraries(${name} PRIVATE ebisu_catch ${T_LINK})
    if(T_INCLUDES)
        target_include_directories(${name} PRIVATE ${T_INCLUDES})
    endif()
    if(T_DEFINES)
        target_compile_definitions(${name} PRIVATE ${T_DEFINES})
    endif()

    if(NOT T_WORKING_DIR)
        message(FATAL_ERROR "ebisu_add_test_suite(${name}): WORKING_DIR is required")
    endif()

    # `ctest --output-junit` records one entry per suite, which would lose the
    # per-test-case detail the CI report is built from -- that granularity is
    # what names the individual failing assertion on a pull request. Catch's own
    # junit reporter keeps it, so use that when reporting is asked for.
    set(command_args "")
    if(EBISU_TEST_JUNIT)
        list(APPEND command_args -r junit -o ${EBISU_JUNIT_DIR}/${name}.xml)
    endif()

    add_test(NAME ${name}
        COMMAND ${name} ${command_args}
        WORKING_DIRECTORY ${T_WORKING_DIR})
    set_tests_properties(${name} PROPERTIES LABELS "${T_LABEL}")
endfunction()

add_subdirectory(small_test)

# The large tests exercise a live backend, and test_env.h raises #error when
# APP_ID is absent, so they cannot even compile without credentials. Configure
# them only when those are present rather than breaking the default build.
set(EBISU_APP_ID "$ENV{APP_ID}" CACHE STRING "Kii application ID used by the large tests")
set(EBISU_DEFAULT_SITE "$ENV{DEFAULT_SITE}" CACHE STRING "Kii site used by the large tests")

if(EBISU_APP_ID AND EBISU_DEFAULT_SITE)
    add_subdirectory(large_test)
else()
    message(STATUS
        "Large tests disabled: set APP_ID and DEFAULT_SITE in the environment, "
        "or -DEBISU_APP_ID=... -DEBISU_DEFAULT_SITE=..., to enable them")
endif()
