# SetGitVersion.cmake

function(set_git_version_and_generate_header)
    execute_process(
        COMMAND git describe --tags
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT GIT_DESCRIBE)
        message(FATAL_ERROR "Failed to retrieve Git tag.")
    endif()

    string(REGEX MATCH "v[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_STRING ${GIT_DESCRIBE})

    if(NOT VERSION_STRING)
        message(FATAL_ERROR "Invalid Git tag format: ${GIT_DESCRIBE}")
    endif()
    string(REGEX REPLACE "^v" "" VERSION_STRING ${VERSION_STRING})

    message("Git: ${GIT_DESCRIBE}, Version: ${VERSION_STRING}")

    set(version "${VERSION_STRING}" PARENT_SCOPE)
    set(soversion "${VERSION_STRING}" PARENT_SCOPE)
    set(version "${VERSION_STRING}")
    set(soversion "${VERSION_STRING}")
    configure_file(${PROJECT_SOURCE_DIR}/../version/ebisu_version.h.in ${PROJECT_SOURCE_DIR}/include/ebisu_version.h)
endfunction()