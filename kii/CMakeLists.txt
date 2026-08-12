file(GLOB kii_srcs CONFIGURE_DEPENDS ./*.c)
add_library(ebisu_kii_objs OBJECT ${kii_srcs})
target_include_directories(ebisu_kii_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # kii's own sources include its private headers by bare name.
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ebisu_kii_objs PRIVATE ebisu_khc_objs ebisu_jkii_objs ebisu_jsmn_objs)
ebisu_configure_target(ebisu_kii_objs)

# libkii keeps the same contents it has always had -- kii plus khc, jkii and
# jsmn -- so anything linking -lkii alone still resolves. The difference is that
# those sources are now compiled once for the whole project instead of once per
# component, and under one set of flags rather than "-Wall" here and
# "-Wall -std=c99" there.
add_library(kii
    $<TARGET_OBJECTS:ebisu_kii_objs>
    $<TARGET_OBJECTS:ebisu_khc_objs>
    $<TARGET_OBJECTS:ebisu_jkii_objs>
    $<TARGET_OBJECTS:ebisu_jsmn_objs>)
target_include_directories(kii
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
set_target_properties(kii PROPERTIES VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(kii)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kii_task_callback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../khc/include/khc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../khc/include/khc_socket_callback.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/include/jkii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/include/jkii_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../jkii/libs/jsmn/jsmn.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS kii
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
