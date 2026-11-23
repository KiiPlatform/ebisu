# Derives the project version from `git describe --tags`, from what git wrote
# into a source archive, or from version/VERSION where there is one, and
# exposes it as a generated ebisu_version.h.

# Captured at include time so the function does not depend on who calls it.
set(EBISU_VERSION_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(set_git_version_and_generate_header)
    # Must stay numeric major.minor.patch: it feeds the VERSION and SOVERSION
    # target properties, which CMake rejects in any other form.
    set(FALLBACK_VERSION "0.0.0")

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

    # And version/VERSION: this directory, which changes when the file is added
    # or removed, and the file itself, when it is there.
    set(version_file ${EBISU_VERSION_MODULE_DIR}/VERSION)
    set_property(DIRECTORY APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${EBISU_VERSION_MODULE_DIR})
    if(EXISTS ${version_file})
        set_property(DIRECTORY APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS ${version_file})
    endif()

    # A source archive made by git, as GitHub's are, has no repository to ask.
    # git fills archive_version.txt in when it makes one (export-subst, in
    # .gitattributes), with what `git describe --tags` would have said and
    # when the commit was made.
    set(archive_describe "")
    set(archive_time "")
    file(READ ${EBISU_VERSION_MODULE_DIR}/archive_version.txt archive_version)
    if(archive_version MATCHES "describe=([^\n]*)\ntime=([0-9]+)")
        set(archive_describe "${CMAKE_MATCH_1}")
        set(archive_time "${CMAKE_MATCH_2}")
    endif()

    # The version comes from the first of these there is:
    #
    # - version/VERSION. ebisu's repository has none. It is for a copy whose
    #   version git can't tell, and says it over anything git would.
    # - What git wrote into a source archive.
    # - `git describe --tags`, when this is the top of a git working tree:
    #   ebisu's own repository, a clone, a worktree or a submodule of it.
    #   Copied into another project's repository instead, as a library often
    #   is, git would answer with that project's tags.
    #
    # Without any, the version is the fallback, with a warning saying why.
    # Each is set here first, as a function sees its caller's variables.
    set(GIT_DESCRIBE "")
    set(VERSION_STRING "")
    set(described_by "")
    set(why "")
    if(EXISTS ${version_file})
        file(READ ${version_file} GIT_DESCRIBE)
        string(STRIP "${GIT_DESCRIBE}" GIT_DESCRIBE)
        if(NOT GIT_DESCRIBE MATCHES
                "^v?([0-9]+\\.[0-9]+\\.[0-9]+)[-+.0-9A-Za-z]*$")
            message(FATAL_ERROR "${version_file} has to hold a version and "
                "nothing else, as 2.0.1 or v2.0.1-rc.1, not: ${GIT_DESCRIBE}")
        endif()
        set(VERSION_STRING "${CMAKE_MATCH_1}")
        set(described_by "${version_file}")
    else()
        if(archive_time)
            set(GIT_DESCRIBE "${archive_describe}")
            set(described_by "the source archive")
            set(why "the source archive is of a commit no tag comes before")
        else()
            execute_process(
                COMMAND git rev-parse --show-cdup
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                RESULT_VARIABLE cdup_result
                OUTPUT_VARIABLE cdup
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(NOT cdup_result EQUAL 0)
                string(CONCAT why "ebisu is in neither a git repository "
                    "nor a source archive made by git")
            elseif(NOT cdup STREQUAL "")
                string(CONCAT why "ebisu is in another project's git "
                    "repository, whose tags aren't ebisu's")
            else()
                execute_process(
                    COMMAND git describe --tags
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    OUTPUT_VARIABLE GIT_DESCRIBE
                    ERROR_VARIABLE GIT_DESCRIBE_ERROR
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_STRIP_TRAILING_WHITESPACE
                )
                set(described_by "git")
                string(CONCAT why "`git describe --tags` failed "
                    "(${GIT_DESCRIBE_ERROR}), as it does in a clone made "
                    "without the tags")
            endif()
        endif()
        if(GIT_DESCRIBE MATCHES "v([0-9]+\\.[0-9]+\\.[0-9]+)")
            set(VERSION_STRING "${CMAKE_MATCH_1}")
        elseif(NOT GIT_DESCRIBE STREQUAL "")
            string(CONCAT why "${described_by} describes it as "
                "${GIT_DESCRIBE}, which holds no vX.Y.Z")
        endif()
    endif()
    if(VERSION_STRING)
        message(STATUS "Version: ${VERSION_STRING} (${GIT_DESCRIBE}, from "
            "${described_by})")
    else()
        message(WARNING "${why}. ebisu builds as ${FALLBACK_VERSION}, unless "
            "${version_file} says its version, as 2.0.1.")
        set(VERSION_STRING "${FALLBACK_VERSION}")
    endif()

    # The soname carries the major and the minor. An application allocates
    # khc, kii_t, tio_handler_t and the like itself, with the size its headers
    # gave them, so a minor release, which may add to them, breaks the ABI, and
    # a patch release doesn't: an application built against 2.0 doesn't load
    # 2.1's libraries, rather than run with structs of another size, and loads
    # 2.0.1's. 1.x's was the whole major.minor.patch, which had consumers
    # relink for 1.3.1 -> 1.3.2 with nothing about the interface changed.
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" SOVERSION_STRING ${VERSION_STRING})

    # `git describe` output as-is, or what version/VERSION holds, so a build 19
    # commits past v1.3.2 is distinguishable from the release itself. The numeric version cannot carry
    # that: it feeds VERSION and SOVERSION, which CMake requires to be numeric.
    if(NOT GIT_DESCRIBE STREQUAL "")
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
