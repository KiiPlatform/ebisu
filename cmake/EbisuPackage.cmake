# What is installed for other projects to build against: a CMake package, for
# find_package(ebisu), and a pkg-config file for each library.
#
# The package's targets are ebisu::core, ebisu::khc, ebisu::jkii, ebisu::kii and
# ebisu::tio, and, where they were built, ebisu::posix and ebisu::openssl, or
# ebisu::win32 and ebisu::schannel on Windows, the ports, ebisu::loggers, and
# ebisu::dns_probe on POSIX.
# Each library holds its own code alone, and its target, like its pkg-config
# file, brings in the ones it links. Like the libraries, the headers are all
# installed in one directory, which each target adds to the include path.

include(CMakePackageConfigHelpers)

set(EBISU_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/ebisu)

# What the config has to find again for the ports' imported dependencies.
set(EBISU_PACKAGE_NEEDS_THREADS FALSE)
if(TARGET ebisu_posix OR TARGET ebisu_openssl)
    set(EBISU_PACKAGE_NEEDS_THREADS TRUE)
endif()
set(EBISU_PACKAGE_NEEDS_OPENSSL FALSE)
if(TARGET ebisu_openssl)
    set(EBISU_PACKAGE_NEEDS_OPENSSL TRUE)
endif()

install(EXPORT ebisu-targets
    NAMESPACE ebisu::
    DESTINATION ${EBISU_CMAKE_INSTALL_DIR})
configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/ebisu-config.cmake.in
    ${PROJECT_BINARY_DIR}/ebisu-config.cmake
    INSTALL_DESTINATION ${EBISU_CMAKE_INSTALL_DIR})
# A new major version is a new ABI, and may break the API.
write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/ebisu-config-version.cmake
    VERSION ${version}
    COMPATIBILITY SameMajorVersion)
install(FILES
    ${PROJECT_BINARY_DIR}/ebisu-config.cmake
    ${PROJECT_BINARY_DIR}/ebisu-config-version.cmake
    DESTINATION ${EBISU_CMAKE_INSTALL_DIR})

# pkg-config. The prefix is found from where the file is, so that an install
# with `cmake --install --prefix` elsewhere still points at itself.
set(pc_dir ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
file(RELATIVE_PATH pc_prefix_from_dir /root/${pc_dir} /root)
string(REGEX REPLACE "/$" "" pc_prefix_from_dir "${pc_prefix_from_dir}")

function(_ebisu_pkg_config name lib description)
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "LIBS" "REQUIRES;REQUIRES_PRIVATE")
    set(pc_name ${name})
    set(pc_lib ${lib})
    set(pc_description ${description})
    string(REPLACE ";" " " pc_requires "${arg_REQUIRES}")
    string(REPLACE ";" " " pc_requires_private "${arg_REQUIRES_PRIVATE}")
    set(pc_libs_extra "${arg_LIBS}")
    configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ebisu.pc.in
        ${PROJECT_BINARY_DIR}/pkgconfig/ebisu-${name}.pc @ONLY)
    install(FILES ${PROJECT_BINARY_DIR}/pkgconfig/ebisu-${name}.pc DESTINATION ${pc_dir})
endfunction()

# What each requires is what its headers include, and jkii's sources use the
# core without its headers including it.
_ebisu_pkg_config(core ebisu_core
    "Core of ebisu: the abstractions it is written against, and its utilities")
_ebisu_pkg_config(khc ebisu_khc "HTTP client of ebisu" REQUIRES ebisu-core)
_ebisu_pkg_config(jkii ebisu_jkii "JSON parser of ebisu" REQUIRES_PRIVATE ebisu-core)
_ebisu_pkg_config(kii ebisu_kii "Kii Cloud client of ebisu"
    REQUIRES ebisu-khc ebisu-jkii ebisu-core)
_ebisu_pkg_config(tio ebisu_tio "Thing-IF client of ebisu" REQUIRES ebisu-kii ebisu-jkii)
if(TARGET ebisu_posix)
    _ebisu_pkg_config(posix ebisu_posix
        "POSIX port of ebisu: the system and locks on POSIX threads"
        REQUIRES ebisu-core LIBS -pthread)
endif()
if(TARGET ebisu_openssl)
    _ebisu_pkg_config(openssl ebisu_openssl
        "OpenSSL port of ebisu: sockets over TLS with OpenSSL"
        REQUIRES ebisu-core openssl LIBS -pthread)
endif()
if(TARGET ebisu_win32)
    _ebisu_pkg_config(win32 ebisu_win32
        "Win32 port of ebisu: the system and locks on the Win32 API"
        REQUIRES ebisu-core)
endif()
if(TARGET ebisu_loggers)
    set(loggers_description
        "Loggers of ebisu: to a stdio stream, to syslog, and to a syslog server over UDP")
    if(WIN32)
        _ebisu_pkg_config(loggers ebisu_loggers "${loggers_description}"
            REQUIRES ebisu-core LIBS -lws2_32)
    else()
        _ebisu_pkg_config(loggers ebisu_loggers "${loggers_description}"
            REQUIRES ebisu-core)
    endif()
endif()
if(TARGET ebisu_dns_probe)
    _ebisu_pkg_config(dns-probe ebisu_dns_probe
        "DNS probe of ebisu: whether the system's name servers answer, before a lookup"
        REQUIRES ebisu-core)
endif()
if(TARGET ebisu_schannel)
    _ebisu_pkg_config(schannel ebisu_schannel
        "Schannel port of ebisu: sockets over TLS with Schannel"
        REQUIRES ebisu-core LIBS "-lws2_32 -lsecur32 -lcrypt32")
endif()
