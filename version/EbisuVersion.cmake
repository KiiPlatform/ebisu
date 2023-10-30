# SetGitVersion.cmake

function(set_git_version_and_generate_header)
    execute_process(
        COMMAND git describe --tags
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        ERROR_VARIABLE GIT_DESCRIBE_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(GIT_DESCRIBE)
        string(REGEX MATCH "v[0-9]+\\.[0-9]+\\.[0-9]+" VERSION_STRING ${GIT_DESCRIBE})
        if(VERSION_STRING)
            string(REGEX REPLACE "^v" "" VERSION_STRING ${VERSION_STRING})
            message("Git: ${GIT_DESCRIBE}, Version: ${VERSION_STRING}")
        else()
            message(WARNING "Invalid Git tag format: ${GIT_DESCRIBE}")
            set(VERSION_STRING, "0.0.0-unknown")
        endif()
    else()
        message(WARNING "Failed to retrieve Git tag: ${GIT_DESCRIBE_ERROR}")
        set(VERSION_STRING, "0.0.0-unknown")
    endif()


   

    set(version "${VERSION_STRING}" PARENT_SCOPE)
    set(soversion "${VERSION_STRING}" PARENT_SCOPE)
    set(version "${VERSION_STRING}")
    set(soversion "${VERSION_STRING}")
    configure_file(${PROJECT_SOURCE_DIR}/../version/ebisu_version.h.in ${PROJECT_SOURCE_DIR}/include/ebisu_version.h)
endfunction()