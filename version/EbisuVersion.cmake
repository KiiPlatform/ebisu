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

    # `git describe` runs here, at configure time, and nothing tells the build
    # that its answer has changed: a commit, a checkout or a new tag left the
    # version, the soname and the header as they were until cmake was run again
    # by hand. So configuring again whenever git moves is left to the build, as
    # for a CMakeLists.txt edited. It takes under a second.
    #
    # What moves: HEAD's reflog, appended to on every commit, checkout, reset or
    # rebase; the tags, loose ones in refs/tags, whose directory changes when
    # one is added or removed, and packed ones in packed-refs; and the whole
    # reftable stack, for a repository using that format instead. Asked of git
    # rather than assumed under .git, which a worktree has as a file. Only those
    # that exist are watched: one missing would have some generators configure
    # on every build. Outside a git checkout there is nothing to watch, and the
    # fallback version stays.
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
