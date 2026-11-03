# The core of ebisu: the abstractions the rest is written against -- sockets,
# the system and locks, the clock and loggers -- and the utilities every
# component uses. A library of its own, which jkii, khc, kii and tio link, and
# the ports too, so that there is one copy of it however many link it.
file(GLOB core_srcs CONFIGURE_DEPENDS src/*.c)
# The tests and fuzzers link the objects, as they do every component's.
add_library(ebisu_core_objs OBJECT ${core_srcs})
target_include_directories(ebisu_core_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ebisu_configure_target(ebisu_core_objs)

add_library(ebisu_core $<TARGET_OBJECTS:ebisu_core_objs>)
target_include_directories(ebisu_core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
# Versioned like the others, for the reason given in khc/CMakeLists.txt.
# ebisu::core in the CMake package.
set_target_properties(ebisu_core PROPERTIES
    VERSION ${version} SOVERSION ${soversion} EXPORT_NAME core)
ebisu_configure_target(ebisu_core)

file(GLOB core_hdrs CONFIGURE_DEPENDS include/*.h)
install(FILES ${core_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS ebisu_core EXPORT ebisu-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
