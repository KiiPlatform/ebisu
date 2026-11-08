# The linux sample on the Win32 API, with platform's Windows ports: the system
# and a lock of ebisu_win32, and the sockets of ebisu_schannel, Winsock 2 with
# Schannel for TLS, so it needs nothing beyond what Windows itself provides.

add_executable(win32app example.c)

target_include_directories(win32app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(win32app PRIVATE ebisu_schannel ebisu_win32 ebisu_loggers tio)
# gnu11 without -pedantic, like the linux sample: an application, built against
# the Windows headers, which use Microsoft extensions such as nameless unions.
# mingw-w64's headers mark those so that -pedantic lets them pass, and with
# them this code compiles as -std=c99 -pedantic-errors too, but the Windows
# SDK's headers do not.
ebisu_configure_target(win32app C_STANDARD 11)

# The sample reads state.json from the directory it runs in, so put one next
# to it, and the DLLs too, where Windows looks for them first: tio's and those
# of the libraries below it.
configure_file(state.json ${CMAKE_CURRENT_BINARY_DIR}/state.json COPYONLY)
get_target_property(tio_type tio TYPE)
if(tio_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET win32app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:tio> $<TARGET_FILE:kii> $<TARGET_FILE:khc>
            $<TARGET_FILE:jkii> $<TARGET_FILE:ebisu_core>
            $<TARGET_FILE_DIR:win32app>
        VERBATIM)
endif()
