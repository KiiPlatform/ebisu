#!/bin/bash
# Installs ebisu into a prefix of its own, then builds a program against the
# installed copy twice, through its CMake package and through pkg-config, and
# runs both. The libraries are built with sizes other than the headers', which
# both ways have to define for the program as well. Needs CMake, a C compiler, pkg-config and OpenSSL's headers, for
# the OpenSSL port.
#
# Usage: package_consumer.sh
# With EBISU_CMAKE, the CMake to do it with, as the oldest the project asks
# for: cmake_minimum_required() only says so, and a command or a variable of a
# later one configures all the same with a later one.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
cmake=${EBISU_CMAKE:-cmake}
"$cmake" --version | head -1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

"$cmake" -S "$root" -B "$work/build" -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release \
    -DKII_TOKEN_MAX_SIZE=256 -DKII_HOST_MAX_SIZE=96 -DJKII_WRITER_MAX_DEPTH=20 > "$work/configure.log" \
    || { cat "$work/configure.log"; exit 1; }
"$cmake" --build "$work/build" -j"$(nproc)" > "$work/build.log" || { cat "$work/build.log"; exit 1; }
"$cmake" --install "$work/build" --prefix "$work/prefix" > /dev/null
libdir=$(dirname "$(find "$work/prefix" -name 'ebisu-kii.pc')")/..

# The licence and the notices go with the libraries.
for f in LICENSE NOTICE; do
    cmp "$root/$f" "$work/prefix/share/doc/ebisu/$f"
done

# The major version README.md's example asks the package for. A build that
# says it is another, as master's did while it was becoming 3.0 but was
# described from 2.0's tag, fails both ways below.
major=$(sed -n 's/^find_package(ebisu \([0-9][0-9]*\) CONFIG REQUIRED)$/\1/p' "$root/README.md")
if [ -z "$major" ]; then
    echo "README.md has no find_package(ebisu <major> CONFIG REQUIRED)" >&2
    exit 1
fi

mkdir "$work/consumer"
cat > "$work/consumer/main.c" <<'SOURCE'
#include <ebisu_dns_probe.h>
#include <ebisu_openssl.h>
#include <ebisu_posix.h>
#include <ebisu_version.h>
#include <jkii_writer.h>
#include <kii.h>
#include <stdio.h>

/* What the libraries were built with: otherwise this would lay kii_t and
 * jkii_writer_t out otherwise than they do. */
#if KII_TOKEN_MAX_SIZE != 256 || KII_HOST_MAX_SIZE != 96 || JKII_WRITER_MAX_DEPTH != 20
#error "the sizes the libraries were built with are not defined here"
#endif
#if KII_REFRESH_TOKEN_MAX_SIZE != 128
#error "a size the libraries were built with as kii.h has it is defined otherwise"
#endif

int main(void)
{
    kii_t kii;
    ebisu_openssl_socket_t tls;
    ebisu_dns_probe_socket_t sock;
    kii_init(&kii);
    kii_set_site(&kii, "api-jp.kii.com");
    ebisu_openssl_socket_init(&tls, 5, 5);
    ebisu_dns_probe_socket_init(&sock, &tls.base, ebisu_posix_dns(), 3000);
    kii_set_http_socket(&kii, &sock.base);
    kii_set_system(&kii, ebisu_posix_system());
    printf("ebisu %s\n", EBISU_VERSION);
    return 0;
}
SOURCE
cat > "$work/consumer/CMakeLists.txt" <<SOURCE
cmake_minimum_required(VERSION 3.15)
project(consumer C)
find_package(ebisu $major CONFIG REQUIRED)
add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE ebisu::dns_probe ebisu::openssl ebisu::posix ebisu::kii)
SOURCE

"$cmake" -S "$work/consumer" -B "$work/consumer/build" -DCMAKE_PREFIX_PATH="$work/prefix" > "$work/consumer.log" \
    || { cat "$work/consumer.log"; exit 1; }
"$cmake" --build "$work/consumer/build"
echo "find_package:"
LD_LIBRARY_PATH="$libdir" "$work/consumer/build/consumer"

# The ports first: they call into the library, which comes after them.
version=$(PKG_CONFIG_PATH="$libdir/pkgconfig" pkg-config --modversion ebisu-kii)
echo "$version"
case $version in
    "$major".*) ;;
    *) echo "ebisu-kii is $version, not the $major.x README.md asks for" >&2; exit 1 ;;
esac
cc "$work/consumer/main.c" -o "$work/consumer/consumer-pc" \
    $(PKG_CONFIG_PATH="$libdir/pkgconfig" pkg-config --cflags --libs ebisu-dns-probe ebisu-openssl ebisu-posix ebisu-kii)
echo "pkg-config:"
LD_LIBRARY_PATH="$libdir" "$work/consumer/consumer-pc"
