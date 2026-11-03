file(GLOB jkii_srcs CONFIGURE_DEPENDS src/*.c)
add_library(ebisu_jkii_objs OBJECT ${jkii_srcs})
target_include_directories(ebisu_jkii_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # coreJSON, vendored in libs/coreJSON and private to jkii, which
    # src/jkii_core_json.c compiles under names of jkii's own: libebisu_jkii
    # exports none of it and installs none of its headers.
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libs>)
# For ebisu_str.h.
target_link_libraries(ebisu_jkii_objs PUBLIC ebisu_core_objs)
ebisu_configure_target(ebisu_jkii_objs)

# jkii, coreJSON among its objects. ebisu_core, whose strings and numbers
# jkii's sources can use, is linked, not embedded.
add_library(jkii $<TARGET_OBJECTS:ebisu_jkii_objs>)
target_include_directories(jkii
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(jkii PRIVATE ebisu_core)
# libebisu_jkii, versioned like the others, for the reason given in
# khc/CMakeLists.txt.
set_target_properties(jkii PROPERTIES
    OUTPUT_NAME ebisu_jkii VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(jkii)

file(GLOB jkii_hdrs CONFIGURE_DEPENDS include/*.h)
# See the note in khc/CMakeLists.txt: a stale in-source ebisu_version.h from a
# pre-move checkout must not be installed over the generated one.
list(REMOVE_ITEM jkii_hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/ebisu_version.h)
install(FILES ${jkii_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS jkii EXPORT ebisu-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
