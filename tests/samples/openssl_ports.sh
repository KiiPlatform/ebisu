#!/bin/bash
# Builds platform's OpenSSL and POSIX ports against the oldest OpenSSL of each
# series the target devices have, which no runner has, and runs their tests.
# The OpenSSL is built from its release tarball, checked against the SHA-256
# openssl.org publishes, and the ports as the libraries' CMake builds them --
# C99, -pedantic-errors, -Wall -Werror, hidden visibility -- in Ubuntu 18.04.
# By hand, as CMake there is too old for the project.
#
# Usage: openssl_ports.sh <version>
#   1.0.2p, the oldest 1.0.2 of the targets, or 1.1.1i, the oldest 1.1.1. A
#   later release of the same series has the same API.
# DOCKER names the container tool, docker by default; DOCKER_OPTS adds options
# to its run, such as --security-opt label=disable for podman with SELinux.
set -euo pipefail
version=$1
root=$(cd "$(dirname "$0")/../.." && pwd)

case $version in
    1.0.2p) sha256=50a98e07b1a89eb8f6a99477f262df71c6fa7bef77df4dc83025a2845c827d00 ;;
    1.1.1i) sha256=e8be6a35fe41d10603c3cc635e93289ed00bf34b79671a3a4de64fcee00d5242 ;;
    *) echo "no checksum for OpenSSL $version" >&2; exit 2 ;;
esac

if [ "${OPENSSL_PORTS_INSIDE:-}" != 1 ]; then
    # The version header, which needs git, is written out here.
    out=$(mktemp -d)
    trap 'rm -rf "$out"' EXIT
    "$root/version/ebisu_version.sh" "$out/ebisu_version.h"
    # shellcheck disable=SC2086
    "${DOCKER:-docker}" run --rm ${DOCKER_OPTS:-} -e OPENSSL_PORTS_INSIDE=1 \
        -v "$root:/src:ro" -v "$out:/out" docker.io/library/ubuntu:18.04 \
        bash /src/tests/samples/openssl_ports.sh "$version"
    exit
fi

apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends build-essential perl wget ca-certificates >/dev/null

series=${version%[a-z]}
cd /tmp
wget -q "https://www.openssl.org/source/old/$series/openssl-$version.tar.gz"
echo "$sha256  openssl-$version.tar.gz" | sha256sum -c -
tar xzf "openssl-$version.tar.gz"
cd "openssl-$version"
./config --prefix=/opt/openssl --openssldir=/opt/openssl/ssl no-shared -fPIC > /tmp/openssl-config.log
if [ "$series" = 1.0.2 ]; then
    make depend > /tmp/openssl-depend.log 2>&1
fi
make -j"$(nproc)" > /tmp/openssl-build.log 2>&1 || { tail -30 /tmp/openssl-build.log; exit 1; }
make install_sw > /tmp/openssl-install.log
/opt/openssl/bin/openssl version

cflags=(-std=c99 -pedantic-errors -Wall -Werror -fvisibility=hidden -fPIC -D_DEFAULT_SOURCE)
inc=(-I/opt/openssl/include -I/src/core/include -I/src/platform/openssl/include
    -I/src/platform/posix/include -I/out)
tinc=("${inc[@]}" -I/src/tests -I/src/tests/large_test -I/src/kii/include -I/src/kii
    -I/src/khc/include -I/src/jkii/include)
libs=(/opt/openssl/lib/libssl.a /opt/openssl/lib/libcrypto.a -ldl -lpthread)
mkdir -p /tmp/b
cd /tmp/b
gcc "${cflags[@]}" "${inc[@]}" -c /src/core/src/ebisu_clock.c -o clock.o
gcc "${cflags[@]}" "${inc[@]}" -c /src/platform/openssl/src/ebisu_openssl.c -o openssl.o
gcc "${cflags[@]}" "${inc[@]}" -c /src/platform/posix/src/ebisu_posix.c -o posix.o

printf '#define CATCH_CONFIG_MAIN\n#include "catch.hpp"\n' > main.cpp
g++ -std=c++11 "${tinc[@]}" -c main.cpp -o main.o
g++ -std=c++11 "${tinc[@]}" -c /src/tests/large_test/kii/openssl_socket_test.cpp -o openssl_test.o
g++ -std=c++11 "${tinc[@]}" -c /src/tests/small_test/tio/posix_test.cpp -o posix_test.o
g++ main.o openssl_test.o clock.o openssl.o "${libs[@]}" -o openssl_test
g++ main.o posix_test.o posix.o -lpthread -o posix_test

echo "OpenSSL port:"
timeout 300 ./openssl_test
echo "POSIX port:"
timeout 120 ./posix_test
