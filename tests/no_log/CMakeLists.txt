# The ports as -DEBISU_LOG=OFF, or EBISU_NO_LOG, builds them: they have to
# compile under the libraries' settings, and keep none of what they log. Each
# is built twice, with logging and without, and a test looks for its messages
# in each object: there has to be some in the first, so that the look finds
# them where they are, and none in the second.

# ebisu_check_no_log(<port> <source>)
function(ebisu_check_no_log port source)
  get_target_property(defs ${port} COMPILE_DEFINITIONS)
  if(NOT defs)
    set(defs "")
  endif()
  foreach(variant logging no_log)
    set(objs ebisu_${variant}_${port})
    add_library(${objs} OBJECT ${source})
    # The port's include directories, and the definitions its source is
    # compiled with.
    target_link_libraries(${objs} PRIVATE ${port})
    target_compile_definitions(${objs} PRIVATE ${defs})
    if(variant STREQUAL "no_log")
      target_compile_definitions(${objs} PRIVATE EBISU_NO_LOG)
    endif()
    ebisu_configure_target(${objs})
  endforeach()
  add_test(NAME no_log_${port}
    COMMAND ${CMAKE_COMMAND}
      -DLOGGING=$<TARGET_OBJECTS:ebisu_logging_${port}>
      -DNO_LOG=$<TARGET_OBJECTS:ebisu_no_log_${port}>
      -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_log.cmake)
  set_tests_properties(no_log_${port} PROPERTIES LABELS small)
endfunction()

if(TARGET ebisu_openssl)
  ebisu_check_no_log(ebisu_openssl ${PROJECT_SOURCE_DIR}/platform/openssl/src/ebisu_openssl.c)
endif()
if(TARGET ebisu_schannel)
  ebisu_check_no_log(ebisu_schannel ${PROJECT_SOURCE_DIR}/platform/schannel/src/ebisu_schannel.c)
endif()
