#!/bin/sh
# Downloads an mbedTLS release, checks it against its hash, and builds and
# installs it into a prefix, with threads, as the mbedTLS port's sockets of a
# configuration need when they are used on more than one task at once.
#
# Usage: tests/mbedtls/install_mbedtls.sh <version> <prefix>
#   version: 3.6.7 or 4.2.0
set -eu
version=$1
prefix=$2
case "$version" in
    3.6.7) sha256=a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6 ;;
    4.2.0) sha256=2bed9d713b4668f76553b097e72b8aa30bc8f112a940d7ae228d524bbde6ffea ;;
    *) echo "no hash for mbedTLS $version" >&2; exit 2 ;;
esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
archive="$work/mbedtls-$version.tar.bz2"
curl -fsSL --retry 3 -o "$archive" \
    "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$version/mbedtls-$version.tar.bz2"
echo "$sha256  $archive" | sha256sum -c -
tar -xjf "$archive" -C "$work"
src="$work/mbedtls-$version"
# Threads, with POSIX threads: options of mbedTLS's own configuration in 3.6,
# and of TF-PSA-Crypto's in 4.x.
case "$version" in
    3.*) config="$src/scripts/config.py" ;;
    *) config="$src/tf-psa-crypto/scripts/config.py" ;;
esac
python3 "$config" set MBEDTLS_THREADING_C
python3 "$config" set MBEDTLS_THREADING_PTHREAD
cmake -S "$src" -B "$work/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON > "$work/configure.log" ||
    { cat "$work/configure.log" >&2; exit 1; }
cmake --build "$work/build" -j "$(nproc)" > "$work/build.log" || { tail -50 "$work/build.log" >&2; exit 1; }
cmake --install "$work/build" > /dev/null
echo "mbedTLS $version in $prefix"
