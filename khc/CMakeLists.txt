file(GLOB khc_srcs CONFIGURE_DEPENDS src/*.c)
add_library(ebisu_khc_objs OBJECT ${khc_srcs})
target_include_directories(ebisu_khc_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
if(KHC_TRACE)
    # Used only by khc/src/khc_state_impl.c. Previously added with
    # ADD_DEFINITIONS in three separate components, which meant the khc sources
    # were compiled with it once per component.
    target_compile_definitions(ebisu_khc_objs PRIVATE KHC_TRACE)
endif()
ebisu_configure_target(ebisu_khc_objs)

add_library(khc $<TARGET_OBJECTS:ebisu_khc_objs>)
target_include_directories(khc
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ebisu_configure_target(khc)

file(GLOB khc_hdrs CONFIGURE_DEPENDS include/*.h)
# Older builds generated ebisu_version.h into the source tree. It is gitignored,
# so a checkout that predates the move still has one lying here, and installing
# it would shadow the freshly generated header with a stale version string.
list(REMOVE_ITEM khc_hdrs ${CMAKE_CURRENT_SOURCE_DIR}/include/ebisu_version.h)
install(FILES ${khc_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS khc
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
