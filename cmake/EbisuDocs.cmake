# Doxygen documentation, one set per component, cross-linked via tag files.
#
# Replaces the per-component `make doc` targets. Those left OUTPUT_DIRECTORY
# empty, so doxygen wrote into the source tree and CI had to rsync the results
# out of it; here each component writes straight into the publish layout, which
# is what the rsync step was reconstructing.

find_package(Doxygen)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found: the `docs` target is unavailable")
    return()
endif()

# Published as <root>/<component>/html, matching the URLs the existing
# ebisu-doc site already serves.
set(EBISU_DOCS_ROOT ${CMAKE_BINARY_DIR}/ebisu-doc
    CACHE PATH "Where the generated documentation is written")

# On now that the public headers document what they export. Obsolete-tag
# warnings, which differ between doxygen releases, do not fail the build --
# only warnings about the documentation itself do.
option(EBISU_DOCS_WARN_AS_ERROR
    "Treat doxygen warnings, including undocumented members, as errors" ON)

# tio's Doxyfile listed kii/kii-core/kii_socket_callback.h and
# kii/kii/kii_task_callback.h, paths from a layout that has not existed for
# years, so those headers were silently missing from the published docs.
# Doxygen only warns about that, and nothing was reading the warnings.
function(_ebisu_check_doc_inputs name)
    file(STRINGS ${PROJECT_SOURCE_DIR}/${name}/Doxyfile.in input_line
        REGEX "^INPUT[ \t]*=")
    string(REGEX REPLACE "^INPUT[ \t]*=[ \t]*" "" input_line "${input_line}")
    string(REPLACE " " ";" inputs "${input_line}")
    foreach(path IN LISTS inputs)
        if(path AND NOT EXISTS ${PROJECT_SOURCE_DIR}/${name}/${path})
            message(FATAL_ERROR
                "${name}/Doxyfile.in lists INPUT path '${path}', which does not exist")
        endif()
    endforeach()
endfunction()

add_custom_target(docs COMMENT "Generating API documentation")

# ebisu_add_docs(<component> [DEPENDS <component>...])
#
# DEPENDS names the components whose tag files this one should read, so that
# types it inherits through its public headers resolve to links rather than
# plain text. kii.h includes khc.h and <jkii.h>, and tio.h includes all three,
# so without this most cross-component references render as dead text.
function(ebisu_add_docs name)
    cmake_parse_arguments(D "" "" "DEPENDS" ${ARGN})

    _ebisu_check_doc_inputs(${name})

    set(EBISU_DOC_VERSION ${version})
    set(EBISU_DOC_OUTPUT ${EBISU_DOCS_ROOT}/${name})
    set(EBISU_DOC_TAGFILE ${EBISU_DOCS_ROOT}/${name}.tag)
    if(EBISU_DOCS_WARN_AS_ERROR)
        set(EBISU_DOC_WARN_AS_ERROR YES)
    else()
        set(EBISU_DOC_WARN_AS_ERROR NO)
    endif()

    # "<tagfile>=<location of that component's html, relative to ours>".
    set(EBISU_DOC_TAGFILES "")
    foreach(dep IN LISTS D_DEPENDS)
        string(APPEND EBISU_DOC_TAGFILES
            " ${EBISU_DOCS_ROOT}/${dep}.tag=../../${dep}/html")
    endforeach()
    string(STRIP "${EBISU_DOC_TAGFILES}" EBISU_DOC_TAGFILES)

    set(doxyfile ${CMAKE_BINARY_DIR}/doxygen/${name}.Doxyfile)
    configure_file(${PROJECT_SOURCE_DIR}/${name}/Doxyfile.in ${doxyfile} @ONLY)

    add_custom_target(docs-${name}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EBISU_DOCS_ROOT}
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        # Doxygen resolves INPUT relative to its working directory.
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/${name}
        COMMENT "Generating ${name} documentation"
        VERBATIM)

    # A component must be generated after the ones whose tag files it reads.
    foreach(dep IN LISTS D_DEPENDS)
        add_dependencies(docs-${name} docs-${dep})
    endforeach()

    add_dependencies(docs docs-${name})
endfunction()

ebisu_add_docs(khc)
ebisu_add_docs(jkii)
ebisu_add_docs(kii DEPENDS khc jkii)
ebisu_add_docs(tio DEPENDS kii khc jkii)
