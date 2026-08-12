cmake_minimum_required(VERSION 3.12)

project(ebisu C)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/version)
include(EbisuVersion)
set_git_version_and_generate_header()

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(KHC_TRACE "Trace KHC activity to stdout" ON)
option(TIO_TRACE "Trace TIO activity to stdout" ON)

# Applies the settings every Ebisu target shares.
#
# Deliberately per-target rather than assigning CMAKE_C_FLAGS: that variable
# used to be overwritten outright by each component, discarding anything the
# toolchain file, the packager, or the developer had put there, and its
# _RELEASE form replaced CMake's "-O3 -DNDEBUG" with a bare "-O2", so NDEBUG
# was never defined in a release build.
function(ebisu_configure_target tgt)
  target_compile_features(${tgt} PRIVATE c_std_99)
  target_compile_options(${tgt} PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>)
  # Every object may end up inside a shared library, including the ones that
  # reach it indirectly through $<TARGET_OBJECTS:...>.
  set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_link_libraries(${tgt} PUBLIC ebisu_version)
endfunction()

add_subdirectory(jkii)
add_subdirectory(khc)
add_subdirectory(kii)
add_subdirectory(tio)

# Tests are for developing ebisu itself. Skipped when ebisu is embedded in
# another project, which is how the vendor SDK builds consume it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Matches what the per-directory Makefiles passed for MEMCHECK=1. Set before
    # include(CTest), which is what reads it.
    # --error-exitcode is the important part. Without it valgrind exits with the
    # program's own status, so `ctest -T memcheck` counted defects, printed them
    # and passed anyway: small_test_tio reported ten for a long time without
    # ever failing a build. Add --track-origins=yes when diagnosing one.
    set(MEMORYCHECK_COMMAND_OPTIONS
        "--leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1"
        CACHE STRING "Options passed to valgrind by `ctest -T memcheck`")
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
