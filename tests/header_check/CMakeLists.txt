# Compiles each public header on its own, as an application would include it,
# under the libraries' -std=c99 -pedantic-errors -Wall -Werror.
#
# Applications compile these headers with their own toolchains, often older
# and stricter than ours, and the libraries' sources say little about that:
# each of them includes several headers, and one included after another can
# lean on what that one declared or included without anything noticing. Here
# each header is the first thing in its translation unit. It is included twice,
# which checks its include guard as well.
#
# Each library's headers are compiled against that library's target, so the
# include directories are exactly those an application linking it gets: a
# header that includes one its library does not provide fails here too.
#
# Object libraries, so nothing is linked or run; the check is that they
# compile, as part of the ordinary build.

# ebisu_check_headers(<library> <header>...)
function(ebisu_check_headers lib)
  set(srcs "")
  foreach(header IN LISTS ARGN)
    string(MAKE_C_IDENTIFIER ${header} header_id)
    set(src ${CMAKE_CURRENT_BINARY_DIR}/${lib}/${header_id}.c)
    # Rewritten only when its content changes, so reconfiguring does not
    # recompile it.
    configure_file(header_check.c.in ${src} @ONLY)
    list(APPEND srcs ${src})
  endforeach()
  add_library(ebisu_header_check_${lib} OBJECT ${srcs})
  target_link_libraries(ebisu_header_check_${lib} PRIVATE ${lib})
  ebisu_configure_target(ebisu_header_check_${lib})
endfunction()

# What each library installs of its own; kii and tio install the headers of the
# libraries they contain as well, which are checked against those here. platform
# has no library of its own, so its headers are checked against khc, which
# embeds it.
ebisu_check_headers(khc khc.h ebisu_object.h ebisu_socket.h ebisu_system.h ebisu_clock.h)
ebisu_check_headers(jkii jkii.h jkii_utils.h jsmn.h)
ebisu_check_headers(kii kii.h kii_task_callback.h)
ebisu_check_headers(tio tio.h)
ebisu_check_headers(ebisu_version ebisu_version.h)
# platform's ports, where they are built, against their own libraries.
if(TARGET ebisu_posix)
  ebisu_check_headers(ebisu_posix ebisu_posix.h)
endif()
if(TARGET ebisu_openssl)
  ebisu_check_headers(ebisu_openssl ebisu_openssl.h)
endif()
