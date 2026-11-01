# libFuzzer targets: each one feeds data a server or the network decides into
# the code that reads it, under ASan and UBSan. Built by the fuzz preset, which
# needs clang; tests/fuzz/run.sh runs them.
#
# Linked from the object libraries for the reason the test suites are: these
# call internals the shared libraries don't export.
function(ebisu_add_fuzzer name)
    cmake_parse_arguments(F "" "" "LINK;INCLUDES" ${ARGN})
    add_executable(${name} ${name}.cpp)
    target_compile_features(${name} PRIVATE cxx_std_11)
    target_include_directories(${name} PRIVATE ${F_INCLUDES})
    target_link_libraries(${name} PRIVATE ${F_LINK})
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    add_dependencies(ebisu_fuzzers ${name})
endfunction()

add_custom_target(ebisu_fuzzers)

ebisu_add_fuzzer(fuzz_khc_response LINK ${EBISU_KHC_OBJS})
ebisu_add_fuzzer(fuzz_jkii_parse LINK ${EBISU_JKII_OBJS})
ebisu_add_fuzzer(fuzz_mqtt_task
    LINK ${EBISU_KII_OBJS}
    INCLUDES ${PROJECT_SOURCE_DIR}/kii)
ebisu_add_fuzzer(fuzz_tio_command
    LINK ${EBISU_TIO_OBJS}
    INCLUDES ${PROJECT_SOURCE_DIR}/tio)
