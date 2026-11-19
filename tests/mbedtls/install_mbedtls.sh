#!/bin/sh
# Downloads an mbedTLS release, checks it against its hash, and builds and
# installs it into a prefix, with threads, as the mbedTLS port's sockets of a
# configuration need when they are used on more than one task at once.
#
# Usage: tests/mbedtls/install_mbedtls.sh <version> <prefix>
#   version: 2.4.0, 2.16.12, 2.28.10, 3.6.7 or 4.2.0
#
# mbedTLS 2 installs no CMake package, which ebisu's CMake finds mbedTLS by:
# this writes one, of the libraries it installs, as an application would name
# them for ebisu.
set -eu
version=$1
prefix=$2
case "$version" in
    # The tags' archives: mbedTLS 2's releases before 2.28 have no archives of
    # their own on GitHub.
    2.4.0) sha256=9a1730ce07848d61ba2c2739566a3f61941f648bf626391afa93d1cd659d491b ;;
    2.16.12) sha256=0afb4a4ce5b771f2fb86daee786362fbe48285f05b73cd205f46a224ec031783 ;;
    2.28.10) sha256=c785ddf2ad66976ab429c36dffd4a021491e40f04fe493cfc39d6ed9153bc246 ;;
    3.6.7) sha256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 ;;
    4.2.0) sha256=2bed9d713b4668f76553b097e72b8aa30bc8f112a940d7ae228d524bbde6ffea ;;
    *) echo "no hash for mbedTLS $version" >&2; exit 2 ;;
esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
case "$version" in
    2.*)
        archive="$work/mbedtls-$version.tar.gz"
        curl -fsSL --retry 3 -o "$archive" \
            "https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/mbedtls-$version.tar.gz"
        echo "$sha256  $archive" | sha256sum -c -
        tar -xzf "$archive" -C "$work"
        src="$work/mbedtls-mbedtls-$version"
        ;;
    *)
        archive="$work/mbedtls-$version.tar.bz2"
        curl -fsSL --retry 3 -o "$archive" \
            "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$version/mbedtls-$version.tar.bz2"
        echo "$sha256  $archive" | sha256sum -c -
        tar -xjf "$archive" -C "$work"
        src="$work/mbedtls-$version"
        ;;
esac
# Threads, with POSIX threads: options of mbedTLS's own configuration, set
# with its config.pl in 2, and of TF-PSA-Crypto's in 4.x.
case "$version" in
    2.*) set_option() { perl "$src/scripts/config.pl" -f "$src/include/mbedtls/config.h" set "$1"; } ;;
    3.*) set_option() { python3 "$src/scripts/config.py" set "$1"; } ;;
    *) set_option() { python3 "$src/tf-psa-crypto/scripts/config.py" set "$1"; } ;;
esac
set_option MBEDTLS_THREADING_C
set_option MBEDTLS_THREADING_PTHREAD
# mbedTLS 2's CMakeLists.txt asks for CMake 2.6 or 2.8, which CMake 4 takes
# only with a policy version it can still keep to.
cmake -S "$src" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON > "$work/configure.log" ||
    { cat "$work/configure.log" >&2; exit 1; }
cmake --build "$work/build" -j "$(nproc)" > "$work/build.log" || { tail -50 "$work/build.log" >&2; exit 1; }
cmake --install "$work/build" > /dev/null
case "$version" in
    2.*)
        mkdir -p "$prefix/lib/cmake/MbedTLS"
        cat > "$prefix/lib/cmake/MbedTLS/MbedTLSConfig.cmake" <<'CONFIG'
# Written by ebisu's tests/mbedtls/install_mbedtls.sh, as mbedTLS 2 installs no
# CMake package: its static libraries, by the names mbedTLS 3's package gives
# them, with POSIX threads, which it is built with.
include(CMakeFindDependencyMacro)
find_dependency(Threads)
get_filename_component(_mbedtls_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
foreach(_mbedtls_lib mbedcrypto mbedx509 mbedtls)
    if(NOT TARGET MbedTLS::${_mbedtls_lib})
        # In lib, or in lib64 where 2.28's GNUInstallDirs put it there.
        find_library(_mbedtls_path ${_mbedtls_lib}
            PATHS "${_mbedtls_prefix}/lib" "${_mbedtls_prefix}/lib64" NO_DEFAULT_PATH)
        add_library(MbedTLS::${_mbedtls_lib} STATIC IMPORTED)
        set_target_properties(MbedTLS::${_mbedtls_lib} PROPERTIES
            IMPORTED_LOCATION "${_mbedtls_path}"
            INTERFACE_INCLUDE_DIRECTORIES "${_mbedtls_prefix}/include")
        unset(_mbedtls_path CACHE)
    endif()
endforeach()
set_target_properties(MbedTLS::mbedcrypto PROPERTIES INTERFACE_LINK_LIBRARIES Threads::Threads)
set_target_properties(MbedTLS::mbedx509 PROPERTIES INTERFACE_LINK_LIBRARIES MbedTLS::mbedcrypto)
set_target_properties(MbedTLS::mbedtls PROPERTIES INTERFACE_LINK_LIBRARIES MbedTLS::mbedx509)
CONFIG
        ;;
esac
echo "mbedTLS $version in $prefix"
