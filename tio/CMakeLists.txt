file(GLOB tio_srcs CONFIGURE_DEPENDS ./*.c)
add_library(ebisu_tio_objs OBJECT ${tio_srcs})
target_include_directories(ebisu_tio_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ebisu_tio_objs
    PRIVATE ebisu_kii_objs ebisu_khc_objs ebisu_core_objs ebisu_jkii_objs ebisu_jsmn_objs)
if(TIO_TRACE)
    # Used only by tio/command_parser.c. This never took effect before: the
    # option was declared as KHC_TRACE a second time by mistake, while the
    # guard tested TIO_TRACE, which nothing defined.
    target_compile_definitions(ebisu_tio_objs PRIVATE TIO_TRACE)
endif()
ebisu_configure_target(ebisu_tio_objs)

# tio alone, linking kii and what kii links.
add_library(tio $<TARGET_OBJECTS:ebisu_tio_objs>)
target_include_directories(tio
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# tio.h includes kii.h, and tio's sources call jkii and jsmn as well.
target_link_libraries(tio PUBLIC kii jkii)
set_target_properties(tio PROPERTIES
    OUTPUT_NAME ebisu_tio VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(tio)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tio.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS tio EXPORT ebisu-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
