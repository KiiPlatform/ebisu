# Derives the project version from `git describe --tags` and exposes it as a
# generated ebisu_version.h.

# Captured at include time so the function does not depend on who calls it.
set(EBISU_VERSION_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(set_git_version_and_generate_header)
    # Must stay numeric major.minor.patch: it feeds the VERSION and SOVERSION
    # target properties, which CMake rejects in any other form.
    set(FALLBACK_VERSION "0.0.0")

    execute_process(
        COMMAND git describe --tags
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        ERROR_VARIABLE GIT_DESCRIBE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # `git describe` runs at configure time only, and nothing told the build
    # when its answer changed: after a commit, a checkout or a new tag, the
    # version, the soname and ebisu_version.h stayed as they were until cmake
    # was run again by hand. So the build configures again whenever git moves,
    # as it does when a CMakeLists.txt is edited. That takes under a second.
    #
    # What moves: HEAD's reflog, written on every commit, checkout, reset or
    # rebase; the tags, loose ones in refs/tags, a directory that changes when
    # one is added or removed, and packed ones in packed-refs; and, in a
    # repository using the reftable format instead, its table list, rewritten
    # on any change. Asked of git rather than assumed under .git, which is a
    # file in a worktree. Only those that exist are watched. Outside a git
    # checkout there is nothing to watch, and the fallback version stays.
    execute_process(
        COMMAND git rev-parse
            --git-path logs/HEAD
            --git-path packed-refs
            --git-path refs/tags
            --git-path reftable/tables.list
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_PATHS
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_PATHS)
        string(REPLACE "\n" ";" GIT_PATHS "${GIT_PATHS}")
        foreach(git_path IN LISTS GIT_PATHS)
            get_filename_component(git_path "${git_path}" ABSOLUTE
                BASE_DIR ${PROJECT_SOURCE_DIR})
            if(EXISTS "${git_path}")
                set_property(DIRECTORY APPEND
                    PROPERTY CMAKE_CONFIGURE_DEPENDS "${git_path}")
            endif()
        endforeach()
    endif()

    if(GIT_DESCRIBE)
        string(REGEX MATCH "v[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_STRING ${GIT_DESCRIBE})
        if(VERSION_STRING)
            string(REGEX REPLACE "^v" "" VERSION_STRING ${VERSION_STRING})
            message(STATUS "Git: ${GIT_DESCRIBE}, Version: ${VERSION_STRING}")
        else()
            message(WARNING "Invalid Git tag format: ${GIT_DESCRIBE}, using ${FALLBACK_VERSION}")
            set(VERSION_STRING "${FALLBACK_VERSION}")
        endif()
    else()
        message(WARNING "Failed to retrieve Git tag: ${GIT_DESCRIBE_ERROR}, using ${FALLBACK_VERSION}")
        set(VERSION_STRING "${FALLBACK_VERSION}")
    endif()

    # The soname carries the major only. It used to be the whole
    # major.minor.patch, which says that every patch release breaks the ABI:
    # consumers had to relink for 1.3.1 -> 1.3.2 even though nothing about the
    # interface had changed, and conversely there was no way to signal that it
    # actually had.
    string(REGEX MATCH "^[0-9]+" SOVERSION_STRING ${VERSION_STRING})

    # `git describe` output as-is, so a build 19 commits past v1.3.2 is
    # distinguishable from the release itself. The numeric version cannot carry
    # that: it feeds VERSION and SOVERSION, which CMake requires to be numeric.
    if(GIT_DESCRIBE)
        set(VERSION_FULL_STRING "${GIT_DESCRIBE}")
    else()
        set(VERSION_FULL_STRING "${VERSION_STRING}")
    endif()

    set(version "${VERSION_STRING}" PARENT_SCOPE)
    set(soversion "${SOVERSION_STRING}" PARENT_SCOPE)
    set(version "${VERSION_STRING}")
    set(soversion "${SOVERSION_STRING}")
    set(version_full "${VERSION_FULL_STRING}")

    # When the source built was committed, in seconds since the epoch. Nothing
    # built from it runs earlier, which lets khc refuse a server's date from
    # before then. Outside git, the day this was written.
    execute_process(
        COMMAND git log -1 --format=%ct
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE source_time
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT source_time MATCHES "^[0-9]+$")
        # 2026-10-27T00:00:00Z
        set(source_time 1793059200)
    endif()

    # Into the build tree, not the source tree. This used to be written to
    # ${PROJECT_SOURCE_DIR}/include/ebisu_version.h -- once per component, so
    # configuring mutated four source directories and two build trees of
    # differing configuration raced over the same files.
    set(generated_dir ${PROJECT_BINARY_DIR}/generated)
    configure_file(
        ${EBISU_VERSION_MODULE_DIR}/ebisu_version.h.in
        ${generated_dir}/ebisu_version.h
        @ONLY)

    add_library(ebisu_version INTERFACE)
    target_include_directories(ebisu_version
        INTERFACE $<BUILD_INTERFACE:${generated_dir}>)
    install(FILES ${generated_dir}/ebisu_version.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endfunction()
