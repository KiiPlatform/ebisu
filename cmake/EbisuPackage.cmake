# What is installed for other projects to build against: a CMake package, for
# find_package(ebisu), and a pkg-config file for each library.
#
# The package's targets are ebisu::khc, ebisu::jkii, ebisu::kii and ebisu::tio,
# and, where they were built, ebisu::posix and ebisu::openssl, or ebisu::win32
# and ebisu::schannel on Windows, the ports. Like
# the libraries, the headers are all installed in one directory, which each
# target adds to the include path. A port carries none of the rest of platform:
# link it with one of the libraries, as the README says.

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
    cmake_parse_arguments(PARSE_ARGV 3 arg "" "REQUIRES;LIBS" "")
    set(pc_name ${name})
    set(pc_lib ${lib})
    set(pc_description ${description})
    set(pc_requires "${arg_REQUIRES}")
    set(pc_libs_extra "${arg_LIBS}")
    configure_file(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ebisu.pc.in
        ${PROJECT_BINARY_DIR}/pkgconfig/ebisu-${name}.pc @ONLY)
    install(FILES ${PROJECT_BINARY_DIR}/pkgconfig/ebisu-${name}.pc DESTINATION ${pc_dir})
endfunction()

_ebisu_pkg_config(khc khc "HTTP client of ebisu")
_ebisu_pkg_config(jkii jkii "JSON parser of ebisu")
_ebisu_pkg_config(kii kii "Kii Cloud client of ebisu, with khc and jkii")
_ebisu_pkg_config(tio tio "Thing-IF client of ebisu, with kii, khc and jkii")
if(TARGET ebisu_posix)
    _ebisu_pkg_config(posix ebisu_posix
        "POSIX port of ebisu's platform: link it before khc, kii or tio"
        LIBS -pthread)
endif()
if(TARGET ebisu_openssl)
    _ebisu_pkg_config(openssl ebisu_openssl
        "OpenSSL port of ebisu's platform: link it before khc, kii or tio"
        REQUIRES openssl LIBS -pthread)
endif()
if(TARGET ebisu_win32)
    _ebisu_pkg_config(win32 ebisu_win32
        "Win32 port of ebisu's platform: link it before khc, kii or tio")
endif()
if(TARGET ebisu_schannel)
    _ebisu_pkg_config(schannel ebisu_schannel
        "Schannel port of ebisu's platform: link it before khc, kii or tio"
        LIBS "-lws2_32 -lsecur32 -lcrypt32")
endif()
