# Not a library of its own: khc, kii and tio each embed it, as they embed what is
# below them, so anything linking one of those alone still resolves.
file(GLOB platform_srcs CONFIGURE_DEPENDS src/*.c)
add_library(ebisu_platform_objs OBJECT ${platform_srcs})
target_include_directories(ebisu_platform_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ebisu_configure_target(ebisu_platform_objs)

file(GLOB platform_hdrs CONFIGURE_DEPENDS include/*.h)
install(FILES ${platform_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
