cmake_minimum_required(VERSION 3.15)

project(ebisu C)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/version ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(EbisuVersion)
set_git_version_and_generate_header()

# Shared libraries by default, where ebisu is what is being built. Inside
# another project, with add_subdirectory(), the choice is that project's, for
# its own libraries too: declared here, the option would make those shared as
# well. Unset, CMake builds static ones.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(BUILD_SHARED_LIBS "Build shared libraries" ON)
endif()
# What ebisu logs itself: the ports' diagnostics, khc's requests, kii's MQTT
# events and tio's failures, to the logger set on each, and nowhere without
# one. OFF leaves all of it out of the libraries, messages and all, for a device
# short of flash, as defining EBISU_NO_LOG does in a build without CMake. An
# application's loggers still take its own messages, and a socket's last_error
# still says why an operation failed.
option(EBISU_LOG "Compile in what ebisu logs" ON)
if(NOT EBISU_LOG)
    add_compile_definitions(EBISU_NO_LOG)
endif()
# How jkii reads a JSON number with a fraction or an exponent: with the C
# library's strtod(), correctly rounded, or, OFF, with code of jkii's own,
# exact for the numbers a device exchanges and a few units in the last place
# off for the longest or the most extreme. OFF leaves strtod() out of a
# program, with the locale support it brings, as defining EBISU_NO_STRTOD does
# in a build without CMake.
option(EBISU_STRTOD "Read doubles with the C library's strtod()" ON)
if(NOT EBISU_STRTOD)
    add_compile_definitions(EBISU_NO_STRTOD)
endif()
# Sessions, kii_session_t: a thing's tokens shared by its kii_t, and renewed
# before they expire and whenever the server refuses one. OFF leaves out what
# attaches a session and renews its token, some 3 KB of a thing's code, for a
# device short of flash, as defining EBISU_NO_SESSION does in a build without
# CMake: kii_set_session(), tio_handler_set_session() and
# tio_updater_set_session() then refuse any session, and a token the server
# refuses fails the request, as it does without one. Nothing else changes, the
# layout of kii_t and kii_session_t included.
option(EBISU_SESSION "Compile in sessions, which renew a thing's tokens" ON)
# TLS session resumption in the ports for devices, the mbedTLS and PolarSSL
# ports': a socket keeps the session of its last connection, for the next to
# the same host to resume, once ebisu_mbedtls_socket_set_session_resume() or
# ebisu_polarssl_socket_set_session_resume() asks it to. And the lwIP socket,
# under them, connects again to the address of its last connection, once
# ebisu_lwip_tcp_socket_set_address_memory() asks it to, as a server whose
# addresses don't share their sessions resumes one only on the address that
# made it.
# OFF leaves out the code and the memory it takes, as not defining
# EBISU_TLS_RESUME does in a build without CMake, which defines it, when it
# does, for the port and for everything using it. The OpenSSL and Schannel
# ports resume sessions whatever this says.
option(EBISU_TLS_RESUME "Let the mbedTLS and PolarSSL ports resume TLS sessions" OFF)
if(NOT EBISU_SESSION)
    add_compile_definitions(EBISU_NO_SESSION)
endif()
# The latest date khc takes from a response's Date header, for
# khc_get_resp_date(), kii_get_resp_date() and the clock given with
# kii_set_clock(), in years after the library's source. A later one says the
# server's clock is wrong, as one before the source does, and is taken for
# none, as tlsdate ignores a time outside its range. 0 takes any. A build
# without CMake defines EBISU_DATE_CEILING_YEARS for the same.
set(EBISU_DATE_CEILING_YEARS 15 CACHE STRING
    "Years after the source past which a response's date is not taken, 0 for none")
if(NOT EBISU_DATE_CEILING_YEARS MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "EBISU_DATE_CEILING_YEARS must be a number of years, 0 or more: ${EBISU_DATE_CEILING_YEARS}")
endif()
add_compile_definitions(EBISU_DATE_CEILING_YEARS=${EBISU_DATE_CEILING_YEARS})

# The sizes of the fixed buffers in kii's types, and how deep jkii's writer
# nests, for a build that needs them otherwise than kii.h and jkii_writer.h
# define them: see kii/README.md and jkii/README.md. They change the layout of
# kii_author_t, kii_mqtt_endpoint_t and kii_t, which tio's handlers hold, and
# of jkii_writer_t, so what is built against the libraries has to define them
# alike. Set here, each is defined for its library and for everything built
# against it: by the library's target, in this build or installed, and by its
# pkg-config file. Empty, as by default, leaves the header's.
set(EBISU_KII_SIZES KII_TOKEN_MAX_SIZE KII_REFRESH_TOKEN_MAX_SIZE
    KII_USERNAME_MAX_SIZE KII_PASSWORD_MAX_SIZE KII_TOPIC_MAX_SIZE
    KII_HOST_MAX_SIZE)
set(EBISU_KII_SIZE_DEFINITIONS "")
set(EBISU_JKII_SIZE_DEFINITIONS "")
foreach(size IN LISTS EBISU_KII_SIZES ITEMS JKII_WRITER_MAX_DEPTH)
    set(${size} "" CACHE STRING "${size}, or empty for the header's")
    # Defined among the compiler's flags instead, it would reach the libraries
    # but not what is built against an installed copy.
    if("${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}" MATCHES "-D *${size}([= ]|$)")
        message(FATAL_ERROR "Set ${size} as a CMake variable, -D${size}=<size>, "
            "rather than among the compiler's flags, so that what is built "
            "against ebisu, installed or not, defines it alike.")
    endif()
    if(NOT "${${size}}" STREQUAL "")
        if(NOT "${${size}}" MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "${size} must be 1 or more: ${${size}}")
        endif()
        if(size STREQUAL "JKII_WRITER_MAX_DEPTH")
            list(APPEND EBISU_JKII_SIZE_DEFINITIONS "${size}=${${size}}")
        else()
            list(APPEND EBISU_KII_SIZE_DEFINITIONS "${size}=${${size}}")
        endif()
    endif()
endforeach()

# On by default so a warning cannot quietly return: the four fixed in #192 sat
# unnoticed for years because the build target only ran doxygen and compiled
# nothing.
#
# If you are packaging ebisu, or building it with a compiler newer than the one
# it was last checked against, a warning we have never seen can stop your build.
# Turn this off rather than patching the source: -DEBISU_WERROR=OFF.
option(EBISU_WERROR "Treat compiler warnings as errors" ON)

# For devices short of flash. The libraries are compiled for size, -Os, with
# each function and each object in a section of its own, so that a link with
# --gc-sections leaves out whatever nothing uses. The shared libraries are
# linked so here; a program linking the static ones has to pass it to its own
# link. The static libraries are also built without position-independent code,
# which costs size and gains nothing on a target with no shared libraries,
# unless CMAKE_POSITION_INDEPENDENT_CODE asks for it, as for linking them into
# a shared library of the application's. See README.md.
option(EBISU_MIN_SIZE "Build the libraries for the smallest code" OFF)

# ebisu_configure_target(<target> [C_STANDARD <version>])
#
# Applies the settings every Ebisu target shares.
#
# Deliberately per-target rather than assigning CMAKE_C_FLAGS: that variable
# used to be overwritten outright by each component, discarding anything the
# toolchain file, the packager, or the developer had put there, and its
# _RELEASE form replaced CMake's "-O3 -DNDEBUG" with a bare "-O2", so NDEBUG
# was never defined in a release build.
#
# The libraries are ISO C99 and nothing else: -std=c99, not gnu99, and
# -pedantic-errors. They are built by vendor SDKs for embedded toolchains, some
# of them old, so a C11 feature, a GNU extension or a POSIX function that slips
# in here breaks somebody else's build rather than ours. This used to be
# target_compile_features(c_std_99), which is only a minimum: CMake adds no flag
# at all when the compiler's default already satisfies it, so the libraries
# were compiled as gnu17 by GCC 13 and as gnu23 by GCC 15, which accept all
# three without a word. Without extensions glibc and newlib also stop declaring
# what ISO C does not have, such as strdup or strncasecmp, so a POSIX dependency
# fails the build too.
#
# C_STANDARD is for the samples, which are applications and not part of the
# libraries: they get the standard they name, with GNU extensions, and without
# -pedantic.
function(ebisu_configure_target tgt)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "C_STANDARD" "")
  if(arg_C_STANDARD)
    set_target_properties(${tgt} PROPERTIES
      C_STANDARD ${arg_C_STANDARD}
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS ON)
  else()
    set_target_properties(${tgt} PROPERTIES
      C_STANDARD 99
      C_STANDARD_REQUIRED ON
      C_EXTENSIONS OFF)
    # Under EBISU_WERROR like any other warning, so that turning it off is
    # still enough to get past a diagnostic a newer compiler has added.
    if(EBISU_WERROR)
      set(pedantic -pedantic-errors)
    else()
      set(pedantic -pedantic)
    endif()
    # C only, should a C++ source ever be built under these settings.
    target_compile_options(${tgt} PRIVATE
      $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:${pedantic}>)
  endif()
  target_compile_options(${tgt} PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
    $<$<AND:$<BOOL:${EBISU_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>)
  set(pic ON)
  if(EBISU_MIN_SIZE)
    # After the build type's flags, so that -Os is the optimization that
    # counts whatever the build type.
    target_compile_options(${tgt} PRIVATE
      "$<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Os;-ffunction-sections;-fdata-sections>")
    get_target_property(type ${tgt} TYPE)
    if(type STREQUAL "SHARED_LIBRARY")
      if(APPLE)
        target_link_options(${tgt} PRIVATE LINKER:-dead_strip)
      elseif(NOT WIN32)
        target_link_options(${tgt} PRIVATE LINKER:--gc-sections)
      endif()
    elseif(NOT BUILD_SHARED_LIBS AND NOT CMAKE_POSITION_INDEPENDENT_CODE)
      set(pic OFF)
    endif()
  endif()
  # Every object may end up inside a shared library, including the ones that
  # reach it indirectly through $<TARGET_OBJECTS:...>, unless EBISU_MIN_SIZE
  # builds them all static, or the target has no shared libraries, as a
  # microcontroller without an operating system doesn't, CMAKE_SYSTEM_NAME
  # Generic. There, position-independent code would only take a global offset
  # table, which a program's linker script and start-up code would have to
  # place and fill, as the FreeRTOS sample's don't.
  get_property(shared_libs GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)
  if(NOT shared_libs AND NOT CMAKE_POSITION_INDEPENDENT_CODE)
    set(pic OFF)
  endif()
  # Hidden by default, so the shared objects export what the public headers
  # declare and nothing else. Being non-static because a helper is called from
  # another translation unit is not the same as being part of the ABI, and
  # without this the two were indistinguishable: libkii exported 203 symbols of
  # which 108 appeared in no installed header. The public headers each push
  # default visibility around their declarations.
  set_target_properties(${tgt} PROPERTIES
    POSITION_INDEPENDENT_CODE ${pic}
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  # Visibility means nothing to a DLL. mingw-w64's linker exports each global
  # function of one in which nothing is marked __declspec(dllexport), as
  # nothing here is, where MSVC's exports none, and makes no import library to
  # link against. So MSVC exports them all too.
  if(MSVC)
    set_target_properties(${tgt} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
  # ebisu_version.h is installed with the other headers, so the target is
  # needed in the build tree only.
  target_link_libraries(${tgt} PUBLIC $<BUILD_INTERFACE:ebisu_version>)
endfunction()

add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(jkii)
add_subdirectory(khc)
add_subdirectory(kii)
add_subdirectory(tio)

include(EbisuPackage)

# The licence, and the notices of what ebisu includes of others', which their
# licences ask to go with every copy: with the libraries, as with the source.
install(FILES LICENSE NOTICE DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Tests and docs are for developing ebisu itself. Skipped when ebisu is embedded
# in another project, which is how the vendor SDK builds consume it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(EbisuDocs)

    # Matches what the per-directory Makefiles passed for MEMCHECK=1. Set before
    # include(CTest), which is what reads it.
    # --error-exitcode is the important part. Without it valgrind exits with the
    # program's own status, so `ctest -T memcheck` counted defects, printed them
    # and passed anyway: small_test_tio reported ten for a long time without
    # ever failing a build. Add --track-origins=yes when diagnosing one.
    # Deliberately not a cache entry: set(... CACHE) leaves an existing entry
    # alone, so changing these would silently have no effect on any build
    # directory configured before the change -- the failure mode being that
    # valgrind quietly keeps running with the old options.
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1")
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()

option(EBISU_BUILD_SAMPLES "Build the sample application for the target platform" OFF)
if(EBISU_BUILD_SAMPLES)
    # The linux sample needs OpenSSL and POSIX threads, and builds on macOS
    # too; the win32 sample needs only what Windows provides.
    if(WIN32)
        add_subdirectory(tio/win32-sample)
    else()
        add_subdirectory(tio/linux-sample)
    endif()
endif()
