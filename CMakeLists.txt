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
