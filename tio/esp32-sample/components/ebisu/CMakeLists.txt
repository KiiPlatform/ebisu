# ebisu's libraries, jkii, khc, kii and tio, as one ESP-IDF component, built
# from the repository's sources.
get_filename_component(ebisu_root ${CMAKE_CURRENT_LIST_DIR}/../../../.. ABSOLUTE)

file(GLOB ebisu_srcs
    ${ebisu_root}/jkii/src/*.c
    ${ebisu_root}/jkii/libs/jsmn/jsmn.c
    ${ebisu_root}/khc/src/*.c
    ${ebisu_root}/kii/*.c
    ${ebisu_root}/tio/*.c)

idf_component_register(
    SRCS ${ebisu_srcs}
    INCLUDE_DIRS
        ${ebisu_root}/jkii/include
        ${ebisu_root}/jkii/libs/jsmn
        ${ebisu_root}/khc/include
        ${ebisu_root}/kii/include
        ${ebisu_root}/tio/include
    PRIV_INCLUDE_DIRS
        ${ebisu_root}/khc/src
        ${ebisu_root}/kii
        ${ebisu_root}/tio)

# ebisu_version.h, generated as the CMake build of the libraries does. Not while
# ESP-IDF only collects the components' requirements, when nothing is built.
if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    # The version comes from git in PROJECT_SOURCE_DIR, and the header goes
    # under PROJECT_BINARY_DIR: ESP-IDF's own here, whose version it would be.
    # ebisu's instead, in this directory only.
    set(PROJECT_SOURCE_DIR ${ebisu_root})
    set(PROJECT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
    include(GNUInstallDirs)
    include(${ebisu_root}/version/EbisuVersion.cmake)
    set_git_version_and_generate_header()
    target_link_libraries(${COMPONENT_LIB} PUBLIC ebisu_version)
endif()
