# The libraries as -DEBISU_LOG=OFF, or EBISU_NO_LOG, builds them: they have to
# compile under the libraries' settings, and keep none of what they log. Each
# is built twice, with logging and without, and a test looks for its messages
# in each object: there has to be some in the first, so that the look finds
# them where they are, and none in the second.

# ebisu_check_no_log(<target> <messages>)
#
# Builds the sources of <target> as it builds them, and checks for <messages>,
# a regular expression. Debug information names functions, variables and enum
# values, so <messages> has to match something only a message has, such as a
# space.
function(ebisu_check_no_log target messages)
  get_target_property(dir ${target} SOURCE_DIR)
  get_target_property(target_sources ${target} SOURCES)
  set(sources "")
  foreach(source IN LISTS target_sources)
    if(NOT IS_ABSOLUTE "${source}")
      set(source "${dir}/${source}")
    endif()
    list(APPEND sources "${source}")
  endforeach()
  foreach(variant logging no_log)
    set(objs ebisu_${variant}_${target})
    add_library(${objs} OBJECT ${sources})
    # The target's include directories and definitions, those of what it
    # links among them.
    target_include_directories(${objs} PRIVATE
      $<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${objs} PRIVATE
      $<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>)
    if(variant STREQUAL "no_log")
      target_compile_definitions(${objs} PRIVATE EBISU_NO_LOG)
    endif()
    ebisu_configure_target(${objs})
  endforeach()
  # The objects as one argument, which a list would not be.
  add_test(NAME no_log_${target}
    COMMAND ${CMAKE_COMMAND}
      "-DMESSAGES=${messages}"
      "-DLOGGING=$<JOIN:$<TARGET_OBJECTS:ebisu_logging_${target}>,|>"
      "-DNO_LOG=$<JOIN:$<TARGET_OBJECTS:ebisu_no_log_${target}>,|>"
      -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_log.cmake)
  set_tests_properties(no_log_${target} PROPERTIES LABELS small)
endfunction()

# What every port logs some of.
set(port_messages "failed to|rejected|timed out|resumed the TLS session")
if(TARGET ebisu_openssl)
  ebisu_check_no_log(ebisu_openssl "${port_messages}")
endif()
if(TARGET ebisu_schannel)
  ebisu_check_no_log(ebisu_schannel "${port_messages}")
endif()
# That no name server answered, and that one answers again.
if(TARGET ebisu_dns_probe)
  ebisu_check_no_log(ebisu_dns_probe "failed to look up |answers again")
endif()
# A request khc makes and what it gets, kii's MQTT events, and what tio fails
# at.
ebisu_check_no_log(ebisu_khc_objs "connected to |a body of | failed")
ebisu_check_no_log(ebisu_kii_objs "MQTT task: ")
ebisu_check_no_log(ebisu_tio_objs "failed to |CMD_PARSE_OK, ")
