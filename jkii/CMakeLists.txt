# jsmn is vendored. libebisu_jkii holds it, as jkii.h includes jsmn.h, and the
# tests link it on its own, so it gets an object library of its own.
file(GLOB jsmn_srcs CONFIGURE_DEPENDS libs/jsmn/*.c)
add_library(ebisu_jsmn_objs OBJECT ${jsmn_srcs})
target_include_directories(ebisu_jsmn_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/jsmn>)
ebisu_configure_target(ebisu_jsmn_objs)

file(GLOB jkii_srcs CONFIGURE_DEPENDS src/*.c)
add_library(ebisu_jkii_objs OBJECT ${jkii_srcs})
target_include_directories(ebisu_jkii_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/jsmn>)
# For ebisu_str.h.
target_link_libraries(ebisu_jkii_objs PUBLIC ebisu_core_objs)
ebisu_configure_target(ebisu_jkii_objs)

# jkii and jsmn, which jkii.h includes. ebisu_core, whose strings and numbers
# jkii's sources can use, is linked, not embedded.
add_library(jkii
    $<TARGET_OBJECTS:ebisu_jkii_objs>
    $<TARGET_OBJECTS:ebisu_jsmn_objs>)
# jkii.h includes <jsmn.h>, so jsmn's directory is part of the public interface.
# Installing flattens these into one include dir, which is why the split never
# showed up before targets were linked directly.
target_include_directories(jkii
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs/jsmn>)
target_link_libraries(jkii PRIVATE ebisu_core)
# libebisu_jkii, versioned like the others, for the reason given in
# khc/CMakeLists.txt.
set_target_properties(jkii PROPERTIES
    OUTPUT_NAME ebisu_jkii VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(jkii)

file(GLOB jkii_hdrs CONFIGURE_DEPENDS include/*.h libs/jsmn/jsmn.h)
# See the note in khc/CMakeLists.txt: a stale in-source ebisu_version.h from a
# pre-move checkout must not be installed over the generated one.
list(REMOVE_ITEM jkii_hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/ebisu_version.h)
install(FILES ${jkii_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS jkii EXPORT ebisu-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
