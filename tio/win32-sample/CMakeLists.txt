# The linux sample on the Win32 API: Winsock 2 for the sockets, Schannel for
# TLS and crypt32 to validate the server's certificate, so it needs nothing
# beyond what Windows itself provides.

set(win32_sample_libs ws2_32 secur32 crypt32)

add_executable(win32app
    example.c
    sock_cb_win32.c
    sys_cb_win32.c)

target_include_directories(win32app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(win32app PRIVATE tio ${win32_sample_libs})
# Without it, windows.h pulls in winsock.h, the 1.1 API, which then clashes
# with winsock2.h.
target_compile_definitions(win32app PRIVATE WIN32_LEAN_AND_MEAN)
# gnu11 without -pedantic, like the linux sample: an application, built against
# the Windows headers, which use Microsoft extensions such as nameless unions.
# mingw-w64's headers mark those so that -pedantic lets them pass, and with
# them this code compiles as -std=c99 -pedantic-errors too, but the Windows
# SDK's headers do not.
ebisu_configure_target(win32app C_STANDARD 11)

# The sample reads state.json from the directory it runs in, so put one next
# to it, and libtio.dll too, where Windows looks for it first.
configure_file(state.json ${CMAKE_CURRENT_BINARY_DIR}/state.json COPYONLY)
get_target_property(tio_type tio TYPE)
if(tio_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET win32app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:tio> $<TARGET_FILE_DIR:win32app>
        VERBATIM)
endif()

# sock_cb_win32.c against TLS servers on 127.0.0.1 that test/tls_test.py
# controls. Only when the tests run under wine: the script adds a CA of its own
# to the root store of a wine prefix it makes for the purpose, which on a
# Windows machine would be its real root store.
if(BUILD_TESTING AND CMAKE_CROSSCOMPILING_EMULATOR)
    find_package(Python3 3.9 COMPONENTS Interpreter)
    find_program(EBISU_OPENSSL_COMMAND openssl)
    if(Python3_FOUND AND EBISU_OPENSSL_COMMAND)
        add_executable(win32app_sock_cb_test
            test/sock_cb_test.c
            sock_cb_win32.c)
        target_include_directories(win32app_sock_cb_test
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(win32app_sock_cb_test
            PRIVATE tio ${win32_sample_libs})
        target_compile_definitions(win32app_sock_cb_test
            PRIVATE WIN32_LEAN_AND_MEAN)
        ebisu_configure_target(win32app_sock_cb_test C_STANDARD 11)

        add_test(NAME win32app_sock_cb
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/test/tls_test.py
                $<TARGET_FILE:win32app_sock_cb_test>
                --wine ${CMAKE_CROSSCOMPILING_EMULATOR}
                --workdir ${CMAKE_CURRENT_BINARY_DIR})
        set_tests_properties(win32app_sock_cb PROPERTIES
            LABELS small
            TIMEOUT 900)
    else()
        message(STATUS
            "win32 sample socket test disabled: it needs python3 and openssl")
    endif()
endif()
