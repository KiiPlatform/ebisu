cmake_minimum_required(VERSION 3.12)

project(ebisu C)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/version ${CMAKE_CURRENT_LIST_DIR}/cmake)
include(EbisuVersion)
set_git_version_and_generate_header()

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
# Debugging output, so off unless asked for. KHC_TRACE prints every request and
# response, headers and bodies included, to stdout; ctest shows a failing
# test's output, so a test run that traces puts that output in its log.
option(KHC_TRACE "Trace KHC activity to stdout" OFF)
option(TIO_TRACE "Trace TIO activity to stdout" OFF)

# On by default so a warning cannot quietly return: the four fixed in #192 sat
# unnoticed for years because the build target only ran doxygen and compiled
# nothing.
#
# If you are packaging ebisu, or building it with a compiler newer than the one
# it was last checked against, a warning we have never seen can stop your build.
# Turn this off rather than patching the source: -DEBISU_WERROR=OFF.
option(EBISU_WERROR "Treat compiler warnings as errors" ON)

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
  # Every object may end up inside a shared library, including the ones that
  # reach it indirectly through $<TARGET_OBJECTS:...>.
  #
  # Hidden by default, so the shared objects export what the public headers
  # declare and nothing else. Being non-static because a helper is called from
  # another translation unit is not the same as being part of the ABI, and
  # without this the two were indistinguishable: libkii exported 203 symbols of
  # which 108 appeared in no installed header. The public headers each push
  # default visibility around their declarations.
  set_target_properties(${tgt} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  target_link_libraries(${tgt} PUBLIC ebisu_version)
endfunction()

add_subdirectory(jkii)
add_subdirectory(khc)
add_subdirectory(kii)
add_subdirectory(tio)

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

# After include(CTest): the win32 sample has a test of its own.
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
