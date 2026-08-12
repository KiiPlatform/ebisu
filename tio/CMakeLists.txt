file(GLOB tio_srcs CONFIGURE_DEPENDS ./*.c)
add_library(ebisu_tio_objs OBJECT ${tio_srcs})
target_include_directories(ebisu_tio_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ebisu_tio_objs
    PRIVATE ebisu_kii_objs ebisu_khc_objs ebisu_jkii_objs ebisu_jsmn_objs)
if(TIO_TRACE)
    # Used only by tio/command_parser.c. This never took effect before: the
    # option was declared as KHC_TRACE a second time by mistake, while the
    # guard tested TIO_TRACE, which nothing defined.
    target_compile_definitions(ebisu_tio_objs PRIVATE TIO_TRACE)
endif()
ebisu_configure_target(ebisu_tio_objs)

# As before, libtio embeds the whole stack.
add_library(tio
    $<TARGET_OBJECTS:ebisu_tio_objs>
    $<TARGET_OBJECTS:ebisu_kii_objs>
    $<TARGET_OBJECTS:ebisu_khc_objs>
    $<TARGET_OBJECTS:ebisu_jkii_objs>
    $<TARGET_OBJECTS:ebisu_jsmn_objs>)
target_include_directories(tio
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set_target_properties(tio PROPERTIES VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(tio)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/tio.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../khc/include/khc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../khc/include/khc_socket_callback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../kii/include/kii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../kii/include/kii_task_callback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/include/jkii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/include/jkii_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/libs/jsmn/jsmn.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS tio
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
