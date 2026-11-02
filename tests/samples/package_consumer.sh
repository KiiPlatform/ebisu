#!/bin/bash
# Installs ebisu into a prefix of its own, then builds a program against the
# installed copy twice, through its CMake package and through pkg-config, and
# runs both. Needs CMake, a C compiler, pkg-config and OpenSSL's headers, for
# the OpenSSL port.
#
# Usage: package_consumer.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cmake -S "$root" -B "$work/build" -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release > "$work/configure.log" \
    || { cat "$work/configure.log"; exit 1; }
cmake --build "$work/build" -j"$(nproc)" > "$work/build.log" || { cat "$work/build.log"; exit 1; }
cmake --install "$work/build" --prefix "$work/prefix" > /dev/null
libdir=$(dirname "$(find "$work/prefix" -name 'ebisu-kii.pc')")/..

mkdir "$work/consumer"
cat > "$work/consumer/main.c" <<'SOURCE'
#include <ebisu_openssl.h>
#include <ebisu_posix.h>
#include <ebisu_version.h>
#include <kii.h>
#include <stdio.h>

int main(void)
{
    kii_t kii;
    ebisu_openssl_socket_t sock;
    kii_init(&kii);
    kii_set_site(&kii, "api-jp.kii.com");
    ebisu_openssl_socket_init(&sock, 5, 5);
    kii_set_http_socket(&kii, &sock.base);
    kii_set_system(&kii, ebisu_posix_system());
    printf("ebisu %s\n", EBISU_VERSION);
    return 0;
}
SOURCE
cat > "$work/consumer/CMakeLists.txt" <<'SOURCE'
cmake_minimum_required(VERSION 3.15)
project(consumer C)
find_package(ebisu CONFIG REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE ebisu::openssl ebisu::posix ebisu::kii)
SOURCE

cmake -S "$work/consumer" -B "$work/consumer/build" -DCMAKE_PREFIX_PATH="$work/prefix" > "$work/consumer.log" \
    || { cat "$work/consumer.log"; exit 1; }
cmake --build "$work/consumer/build"
echo "find_package:"
LD_LIBRARY_PATH="$libdir" "$work/consumer/build/consumer"

# The ports first: they call into the library, which comes after them.
PKG_CONFIG_PATH="$libdir/pkgconfig" pkg-config --modversion ebisu-kii
cc "$work/consumer/main.c" -o "$work/consumer/consumer-pc" \
    $(PKG_CONFIG_PATH="$libdir/pkgconfig" pkg-config --cflags --libs ebisu-openssl ebisu-posix ebisu-kii)
echo "pkg-config:"
LD_LIBRARY_PATH="$libdir" "$work/consumer/consumer-pc"
