#!/bin/bash
# Builds platform's OpenSSL and POSIX ports against an OpenSSL older than the
# runners have, in Ubuntu 18.04, as the libraries' CMake builds them -- C99,
# -pedantic-errors, -Wall -Werror, hidden visibility -- and runs their tests:
# the OpenSSL port supports 1.0.2 and later. By hand, as CMake there is too old
# for the project.
#
# Usage: openssl_ports.sh <package>
#   libssl1.0-dev for OpenSSL 1.0.2, libssl-dev for 1.1.1.
# DOCKER names the container tool, docker by default; DOCKER_OPTS adds options
# to its run, such as --security-opt label=disable for podman with SELinux.
set -euo pipefail
pkg=$1
root=$(cd "$(dirname "$0")/../.." && pwd)

if [ "${OPENSSL_PORTS_INSIDE:-}" != 1 ]; then
    # The version header, which needs git, is written out here.
    out=$(mktemp -d)
    trap 'rm -rf "$out"' EXIT
    "$root/version/ebisu_version.sh" "$out/ebisu_version.h"
    # shellcheck disable=SC2086
    "${DOCKER:-docker}" run --rm ${DOCKER_OPTS:-} -e OPENSSL_PORTS_INSIDE=1 \
        -v "$root:/src:ro" -v "$out:/out" docker.io/library/ubuntu:18.04 \
        bash /src/tests/samples/openssl_ports.sh "$pkg"
    exit
fi

apt-get update -qq >/dev/null
apt-get install -y -qq --no-install-recommends g++ "$pkg" >/dev/null
grep -m1 'OPENSSL_VERSION_TEXT' /usr/include/openssl/opensslv.h

cflags=(-std=c99 -pedantic-errors -Wall -Werror -fvisibility=hidden -fPIC -D_DEFAULT_SOURCE)
inc=(-I/src/platform/include -I/src/platform/openssl/include -I/src/platform/posix/include -I/out)
tinc=("${inc[@]}" -I/src/tests -I/src/tests/large_test -I/src/kii/include -I/src/kii
    -I/src/khc/include -I/src/jkii/include -I/src/jkii/libs/jsmn)
mkdir -p /tmp/b
cd /tmp/b
gcc "${cflags[@]}" "${inc[@]}" -c /src/platform/src/ebisu_clock.c -o clock.o
gcc "${cflags[@]}" "${inc[@]}" -c /src/platform/openssl/src/ebisu_openssl.c -o openssl.o
gcc "${cflags[@]}" "${inc[@]}" -c /src/platform/posix/src/ebisu_posix.c -o posix.o

printf '#define CATCH_CONFIG_MAIN\n#include "catch.hpp"\n' > main.cpp
g++ -std=c++11 "${tinc[@]}" -c main.cpp -o main.o
g++ -std=c++11 "${tinc[@]}" -c /src/tests/large_test/kii/openssl_socket_test.cpp -o openssl_test.o
g++ -std=c++11 "${tinc[@]}" -c /src/tests/small_test/tio/posix_test.cpp -o posix_test.o
g++ main.o openssl_test.o clock.o openssl.o -lssl -lcrypto -lpthread -o openssl_test
g++ main.o posix_test.o posix.o -lpthread -o posix_test

echo "OpenSSL port:"
timeout 300 ./openssl_test
echo "POSIX port:"
timeout 120 ./posix_test
