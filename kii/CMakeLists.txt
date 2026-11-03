file(GLOB kii_srcs CONFIGURE_DEPENDS ./*.c)
add_library(ebisu_kii_objs OBJECT ${kii_srcs})
target_include_directories(ebisu_kii_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    # kii's own sources include its private headers by bare name.
    PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
target_link_libraries(ebisu_kii_objs
    PRIVATE ebisu_khc_objs ebisu_core_objs ebisu_jkii_objs)
ebisu_configure_target(ebisu_kii_objs)

# kii alone: khc, jkii and ebisu_core are libraries of their own, which it
# links.
add_library(kii $<TARGET_OBJECTS:ebisu_kii_objs>)
target_include_directories(kii
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# kii.h includes khc.h, jkii.h and ebisu_core's headers.
target_link_libraries(kii PUBLIC khc jkii ebisu_core)
set_target_properties(kii PROPERTIES
    OUTPUT_NAME ebisu_kii VERSION ${version} SOVERSION ${soversion})
ebisu_configure_target(kii)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kii.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kii_task_callback.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS kii EXPORT ebisu-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
