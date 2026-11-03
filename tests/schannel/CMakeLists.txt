# platform's Schannel port against TLS servers on 127.0.0.1 that tls_test.py
# controls. Only when the tests run under wine: the script adds a CA of its own
# to the root store of a wine prefix it makes for the purpose, which on a
# Windows machine would be its real root store.
find_package(Python3 3.9 COMPONENTS Interpreter)
find_program(EBISU_OPENSSL_COMMAND openssl)
if(Python3_FOUND AND EBISU_OPENSSL_COMMAND)
    add_executable(schannel_socket_test schannel_socket_test.c)
    # crypt32 for trust, which adds the test's CA to the root store.
    target_link_libraries(schannel_socket_test PRIVATE ebisu_schannel crypt32)
    # The port links ebisu_core, which a shared build makes a DLL: its objects
    # here instead, so that there is no library to find under wine.
    target_link_libraries(schannel_socket_test PRIVATE ebisu_core_objs)
    ebisu_configure_target(schannel_socket_test C_STANDARD 11)

    add_test(NAME schannel_socket
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tls_test.py
            $<TARGET_FILE:schannel_socket_test>
            --wine ${CMAKE_CROSSCOMPILING_EMULATOR}
            --workdir ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(schannel_socket PROPERTIES
        LABELS small
        TIMEOUT 900)
else()
    message(STATUS
        "Schannel port test disabled: it needs python3 and openssl")
endif()
