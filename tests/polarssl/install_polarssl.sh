#!/bin/sh
# Downloads a PolarSSL 1.3 release, checks it against its hash, and builds and
# installs it into a prefix, with threads, as the PolarSSL port's sockets of a
# configuration need when they are used on more than one task at once.
#
# Usage: tests/polarssl/install_polarssl.sh <version> <prefix> [no-drbg|no-aead]
#   version: 1.3.8, PolarSSL's, or 1.3.22, the last of mbedTLS 1.3, whose API
#     is PolarSSL's
#   no-drbg: built without POLARSSL_CTR_DRBG_C and POLARSSL_ENTROPY_C, as some
#     of vendors' SDKs configure it, where the application gives TLS its random
#     generator.
#   no-aead: built without POLARSSL_GCM_C and POLARSSL_CCM_C, as some of
#     vendors' SDKs configure it, where no cipher suite has authenticated
#     encryption.
#
# PolarSSL installs no CMake package, which ebisu's CMake finds PolarSSL by:
# this writes one, of the library it installs, as an application would name it
# for ebisu.
set -eu
version=$1
prefix=$2
variant=${3:-}
case "$variant" in
    ''|no-drbg|no-aead) ;;
    *) echo "no variant $variant" >&2; exit 2 ;;
esac
# The tags' archives: PolarSSL's releases have no archives of their own on
# GitHub. 1.3.10 on are mbedTLS's, of the same API.
case "$version" in
    1.3.8) tag=polarssl-1.3.8; sha256=3e96a8a1c54be98e0d74ebc39f4926f432f5c6e83cc8941a6571a537bc0a62af ;;
    1.3.22) tag=mbedtls-1.3.22; sha256=c8f63edd72f9e7a16ac88949a893a57150e7a3242f46b7fec5417ba012bcb2dd ;;
    *) echo "no hash for PolarSSL $version" >&2; exit 2 ;;
esac
# Downloads url to archive and checks it against sha256. With
# EBISU_DOWNLOADS set, a directory, an archive already there with that hash is
# taken in place of the download, and a downloaded one is left there: CI keeps
# the directory in its cache, so that a job doesn't depend on the download.
fetch() {
    url=$1; archive=$2; sha256=$3
    kept=${EBISU_DOWNLOADS:+$EBISU_DOWNLOADS/$(basename "$archive")}
    if [ -n "$kept" ] && [ -f "$kept" ] &&
            echo "$sha256  $kept" | sha256sum -c - > /dev/null 2>&1; then
        cp "$kept" "$archive"
        return
    fi
    curl -fsSL --retry 3 -o "$archive" "$url"
    echo "$sha256  $archive" | sha256sum -c -
    if [ -n "$kept" ]; then
        mkdir -p "$EBISU_DOWNLOADS"
        cp "$archive" "$kept"
    fi
}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
archive="$work/$tag.tar.gz"
fetch "https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/$tag.tar.gz" "$archive" "$sha256"
tar -xzf "$archive" -C "$work"
src="$work/mbedtls-$tag"

# Its configuration's options, one line each in config.h, set or unset: 1.3.8
# has no config.pl to set them with.
config="$src/include/polarssl/config.h"
set_option() {
    python3 -I - "$config" "$1" "$2" <<'OPTION'
import re, sys

path, name, value = sys.argv[1:]
text = open(path).read()
line = re.compile(r'^(//)?#define %s\b.*$' % name, re.M)
found = line.findall(text)
if len(found) != 1:
    sys.exit('%s: no single #define %s' % (path, name))
text = line.sub(('#define %s' if value == 'set' else '//#define %s') % name, text)
open(path, 'w').write(text)
OPTION
}
set_option POLARSSL_THREADING_C set
set_option POLARSSL_THREADING_PTHREAD set
if [ "$variant" = no-drbg ]; then
    set_option POLARSSL_CTR_DRBG_C unset
    set_option POLARSSL_ENTROPY_C unset
fi
if [ "$variant" = no-aead ]; then
    # Session tickets stay: PolarSSL seals them with AES-CBC.
    set_option POLARSSL_GCM_C unset
    set_option POLARSSL_CCM_C unset
fi

# Its CMakeLists.txt asks for CMake 2.6, which CMake 4 takes only with a policy
# version it can still keep to. A compiler of today takes the C of 2014 with
# what it warns of, not as errors: an implicit declaration is one from GCC 14.
cmake -S "$src" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_C_FLAGS="-Wno-error=implicit-function-declaration" \
    -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON > "$work/configure.log" ||
    { cat "$work/configure.log" >&2; exit 1; }
cmake --build "$work/build" -j "$(nproc)" > "$work/build.log" || { tail -50 "$work/build.log" >&2; exit 1; }
cmake --install "$work/build" > /dev/null

mkdir -p "$prefix/lib/cmake/PolarSSL"
cat > "$prefix/lib/cmake/PolarSSL/PolarSSLConfig.cmake" <<'CONFIG'
# Written by ebisu's tests/polarssl/install_polarssl.sh, as PolarSSL installs no
# CMake package: its static library, as PolarSSL::polarssl, with POSIX threads,
# which it is built with.
include(CMakeFindDependencyMacro)
find_dependency(Threads)
get_filename_component(_polarssl_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET PolarSSL::polarssl)
    # libpolarssl, or libmbedtls from 1.3.10, of the same API.
    find_library(_polarssl_path NAMES polarssl mbedtls
        PATHS "${_polarssl_prefix}/lib" "${_polarssl_prefix}/lib64" NO_DEFAULT_PATH)
    add_library(PolarSSL::polarssl STATIC IMPORTED)
    set_target_properties(PolarSSL::polarssl PROPERTIES
        IMPORTED_LOCATION "${_polarssl_path}"
        INTERFACE_INCLUDE_DIRECTORIES "${_polarssl_prefix}/include"
        INTERFACE_LINK_LIBRARIES Threads::Threads)
    unset(_polarssl_path CACHE)
endif()
CONFIG
echo "PolarSSL $version${variant:+ ($variant)} in $prefix"
