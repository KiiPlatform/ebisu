# Ports: ebisu's abstractions implemented for one kind of system each, from
# what that system provides, so each is a library of its own, built where its
# dependencies are found. Static, and linking ebisu_core, which has what a port
# calls into and what its header includes.
if(UNIX)
    find_package(Threads)
    if(Threads_FOUND)
        add_library(ebisu_posix STATIC posix/src/ebisu_posix.c)
        target_include_directories(ebisu_posix
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/posix/include>)
        target_link_libraries(ebisu_posix PUBLIC ebisu_core Threads::Threads)
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
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/openssl/include>)
        # Threads for OpenSSL 1.0.2's locking, and with the dynamic loader's
        # library for a static OpenSSL, as above.
        target_link_libraries(ebisu_openssl
            PUBLIC ebisu_core OpenSSL::SSL OpenSSL::Crypto Threads::Threads ${CMAKE_DL_LIBS})
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

    # The DNS probe: whether the name servers of /etc/resolv.conf answer, over
    # BSD sockets, and a socket that asks before another connects.
    add_library(ebisu_dns_probe STATIC dns_probe/src/ebisu_dns_probe.c)
    target_include_directories(ebisu_dns_probe
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/dns_probe/include>)
    target_link_libraries(ebisu_dns_probe PUBLIC ebisu_core)
    # BSD sockets, poll() and clock_gettime() are POSIX, which glibc declares
    # under -std=c99 only with _DEFAULT_SOURCE.
    target_compile_definitions(ebisu_dns_probe PRIVATE _DEFAULT_SOURCE)
    ebisu_configure_target(ebisu_dns_probe)
    install(FILES dns_probe/include/ebisu_dns_probe.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # ebisu::dns_probe in the CMake package.
    set_target_properties(ebisu_dns_probe PROPERTIES EXPORT_NAME dns_probe)
    install(TARGETS ebisu_dns_probe EXPORT ebisu-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# The Windows ports: the system and a lock on the Win32 API, and TLS over
# Winsock 2 with Schannel, with crypt32 to check the server's certificate, so
# they need nothing beyond what Windows provides.
if(WIN32)
    add_library(ebisu_win32 STATIC win32/src/ebisu_win32.c)
    target_include_directories(ebisu_win32
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/win32/include>)
    target_link_libraries(ebisu_win32 PUBLIC ebisu_core)
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
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/schannel/include>)
    target_link_libraries(ebisu_schannel PUBLIC ebisu_core ws2_32 secur32 crypt32)
    target_compile_definitions(ebisu_schannel PUBLIC WIN32_LEAN_AND_MEAN)
    ebisu_configure_target(ebisu_schannel)
    install(FILES schannel/include/ebisu_schannel.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # ebisu::schannel in the CMake package.
    set_target_properties(ebisu_schannel PROPERTIES EXPORT_NAME schannel)
    install(TARGETS ebisu_schannel EXPORT ebisu-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# Loggers, for a system with a C library and sockets: to a stdio stream, to
# syslog on POSIX, and to a syslog server over UDP, on POSIX and Windows alike.
if(UNIX OR WIN32)
    set(loggers_srcs
        loggers/src/ebisu_loggers_format.c
        loggers/src/ebisu_stdio_logger.c
        loggers/src/ebisu_syslog_udp_logger.c)
    if(UNIX)
        list(APPEND loggers_srcs loggers/src/ebisu_syslog_logger.c)
    endif()
    add_library(ebisu_loggers STATIC ${loggers_srcs})
    target_include_directories(ebisu_loggers
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/loggers/include>)
    target_link_libraries(ebisu_loggers PUBLIC ebisu_core)
    if(WIN32)
        target_link_libraries(ebisu_loggers PUBLIC ws2_32)
        target_compile_definitions(ebisu_loggers PRIVATE WIN32_LEAN_AND_MEAN)
    else()
        # Sockets, gethostname(), clock_gettime() and syslog() are POSIX, which
        # glibc declares under -std=c99 only with _DEFAULT_SOURCE.
        target_compile_definitions(ebisu_loggers PRIVATE _DEFAULT_SOURCE)
    endif()
    ebisu_configure_target(ebisu_loggers)
    install(FILES loggers/include/ebisu_loggers.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # ebisu::loggers in the CMake package.
    set_target_properties(ebisu_loggers PROPERTIES EXPORT_NAME loggers)
    install(TARGETS ebisu_loggers EXPORT ebisu-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
