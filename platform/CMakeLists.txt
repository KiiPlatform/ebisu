# Not a library of its own: khc, kii and tio each embed it, as they embed what is
# below them, so anything linking one of those alone still resolves.
file(GLOB platform_srcs CONFIGURE_DEPENDS src/*.c)
add_library(ebisu_platform_objs OBJECT ${platform_srcs})
target_include_directories(ebisu_platform_objs
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
ebisu_configure_target(ebisu_platform_objs)

file(GLOB platform_hdrs CONFIGURE_DEPENDS include/*.h)
install(FILES ${platform_hdrs} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Ports: the platform implemented for one kind of system each, from what that
# system provides, so each is a library of its own, built where its
# dependencies are found. Static, beside libkhc, libkii or libtio, which have
# the core a port calls into: a port carries none of it, or an application
# linking it with one of those would have two copies.
if(UNIX)
    find_package(Threads)
    if(Threads_FOUND)
        add_library(ebisu_posix STATIC posix/src/ebisu_posix.c)
        target_include_directories(ebisu_posix
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/posix/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
        target_link_libraries(ebisu_posix PUBLIC Threads::Threads)
        # nanosleep() and clock_gettime() are POSIX, which -std=c99 hides:
        # glibc declares them with _DEFAULT_SOURCE, and macOS keeps declaring
        # them unless something asks for strict POSIX.
        target_compile_definitions(ebisu_posix PRIVATE _DEFAULT_SOURCE)
        ebisu_configure_target(ebisu_posix)
        install(FILES posix/include/ebisu_posix.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
        # ebisu::posix in the CMake package.
        set_target_properties(ebisu_posix PROPERTIES EXPORT_NAME posix)
        install(TARGETS ebisu_posix EXPORT ebisu-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()

    # Found is not enough: a 32-bit build on a 64-bit host finds the host's
    # OpenSSL, whose headers and libraries are for the other architecture.
    # A static OpenSSL also needs threads and the dynamic loader's library,
    # which FindOpenSSL leaves out of the libraries it names here, and, in the
    # CMake versions this project supports, can leave out of its targets too.
    find_package(OpenSSL)
    if(OPENSSL_FOUND)
        include(CheckCSourceCompiles)
        set(CMAKE_REQUIRED_INCLUDES ${OPENSSL_INCLUDE_DIR})
        set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY}
            ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
        check_c_source_compiles(
            "#include <openssl/ssl.h>\nint main(void) { return SSL_library_init(); }"
            EBISU_OPENSSL_USABLE)
        unset(CMAKE_REQUIRED_INCLUDES)
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif()
    if(OPENSSL_FOUND AND EBISU_OPENSSL_USABLE AND Threads_FOUND)
        add_library(ebisu_openssl STATIC openssl/src/ebisu_openssl.c)
        target_include_directories(ebisu_openssl
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/openssl/include>
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
        # Threads for OpenSSL 1.0.2's locking, and with the dynamic loader's
        # library for a static OpenSSL, as above.
        target_link_libraries(ebisu_openssl
            PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
        # BSD sockets and hostent's h_addr are POSIX and BSD, which glibc
        # declares under -std=c99 only with _DEFAULT_SOURCE.
        target_compile_definitions(ebisu_openssl PRIVATE _DEFAULT_SOURCE)
        ebisu_configure_target(ebisu_openssl)
        install(FILES openssl/include/ebisu_openssl.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
        # ebisu::openssl in the CMake package.
        set_target_properties(ebisu_openssl PROPERTIES EXPORT_NAME openssl)
        install(TARGETS ebisu_openssl EXPORT ebisu-targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()
endif()

# The Windows ports: the system and a lock on the Win32 API, and TLS over
# Winsock 2 with Schannel, with crypt32 to check the server's certificate, so
# they need nothing beyond what Windows provides.
if(WIN32)
    add_library(ebisu_win32 STATIC win32/src/ebisu_win32.c)
    target_include_directories(ebisu_win32
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/win32/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    # Without it, windows.h pulls in winsock.h, the 1.1 API, which then
    # clashes with winsock2.h in what includes both.
    target_compile_definitions(ebisu_win32 PUBLIC WIN32_LEAN_AND_MEAN)
    ebisu_configure_target(ebisu_win32)
    install(FILES win32/include/ebisu_win32.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # ebisu::win32 in the CMake package.
    set_target_properties(ebisu_win32 PROPERTIES EXPORT_NAME win32)
    install(TARGETS ebisu_win32 EXPORT ebisu-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    add_library(ebisu_schannel STATIC schannel/src/ebisu_schannel.c)
    target_include_directories(ebisu_schannel
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/schannel/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
    target_link_libraries(ebisu_schannel PUBLIC ws2_32 secur32 crypt32)
    target_compile_definitions(ebisu_schannel PUBLIC WIN32_LEAN_AND_MEAN)
    ebisu_configure_target(ebisu_schannel)
    install(FILES schannel/include/ebisu_schannel.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # ebisu::schannel in the CMake package.
    set_target_properties(ebisu_schannel PROPERTIES EXPORT_NAME schannel)
    install(TARGETS ebisu_schannel EXPORT ebisu-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
