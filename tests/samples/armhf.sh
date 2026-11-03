#!/bin/bash
# Cross-compiles ebisu for armhf Linux in Debian stretch, whose glibc, 2.24, is
# older than the oldest the targets have, so nothing built can need a newer
# one, against an OpenSSL built from its release tarball. Builds the
# libraries, platform's ports and the Linux sample, runs the small tests and
# the OpenSSL port's under qemu-arm, fails on any symbol newer than
# GLIBC_2.26, and reports the stripped sizes.
#
# Usage: armhf.sh <OpenSSL version>
#   1.0.2p or 1.1.1i, the oldest of each series the targets have.
# DOCKER names the container tool, docker by default; DOCKER_OPTS adds options
# to its run, such as --security-opt label=disable for podman with SELinux.
set -euo pipefail
version=$1
root=$(cd "$(dirname "$0")/../.." && pwd)
cmake_version=3.28.6
cmake_sha256=931e3c0d546ee03ca72bb147ccd9b49e3b6252f765f66bf21b9d165519940458
# The newest glibc symbol version the binaries may need.
glibc_max=2.26

case $version in
    1.0.2p) openssl_sha256=50a98e07b1a89eb8f6a99477f262df71c6fa7bef77df4dc83025a2845c827d00 ;;
    1.1.1i) openssl_sha256=e8be6a35fe41d10603c3cc635e93289ed00bf34b79671a3a4de64fcee00d5242 ;;
    *) echo "no checksum for OpenSSL $version" >&2; exit 2 ;;
esac

if [ "${ARMHF_INSIDE:-}" != 1 ]; then
    # Downloaded here, where the certificates are current, and checked.
    dl=$(mktemp -d)
    trap 'rm -rf "$dl"' EXIT
    series=${version%[a-z]}
    curl -fsSL -o "$dl/openssl-$version.tar.gz" \
        "https://www.openssl.org/source/old/$series/openssl-$version.tar.gz"
    curl -fsSL -o "$dl/cmake.tar.gz" \
        "https://github.com/Kitware/CMake/releases/download/v$cmake_version/cmake-$cmake_version-linux-x86_64.tar.gz"
    (cd "$dl" && printf '%s  %s\n%s  %s\n' \
        "$openssl_sha256" "openssl-$version.tar.gz" "$cmake_sha256" cmake.tar.gz | sha256sum -c -)
    # shellcheck disable=SC2086
    "${DOCKER:-docker}" run --rm ${DOCKER_OPTS:-} -e ARMHF_INSIDE=1 \
        -v "$root:/src:ro" -v "$dl:/dl:ro" docker.io/library/debian:stretch \
        bash /src/tests/samples/armhf.sh "$version"
    exit
fi

# Stretch has moved to the archive.
cat > /etc/apt/sources.list <<'SOURCES'
deb http://archive.debian.org/debian stretch main
deb http://archive.debian.org/debian-security stretch/updates main
SOURCES
echo 'Acquire::Check-Valid-Until "false";' > /etc/apt/apt.conf.d/99archive
apt-get update -qq > /dev/null
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends \
    crossbuild-essential-armhf qemu-user git make perl file > /dev/null
arm-linux-gnueabihf-gcc --version | sed -n 1p
ldd --version | sed -n 1p

tar xzf /dl/cmake.tar.gz -C /opt
export PATH="/opt/cmake-$cmake_version-linux-x86_64/bin:$PATH"

tar xzf "/dl/openssl-$version.tar.gz" -C /tmp
cd "/tmp/openssl-$version"
./Configure linux-armv4 --cross-compile-prefix=arm-linux-gnueabihf- \
    --prefix=/opt/openssl --openssldir=/opt/openssl/ssl no-shared -fPIC \
    -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard > /tmp/openssl-config.log
if [ "${version%[a-z]}" = 1.0.2 ]; then
    make depend > /tmp/openssl-depend.log 2>&1
fi
make -j"$(nproc)" > /tmp/openssl-build.log 2>&1 || { tail -30 /tmp/openssl-build.log; exit 1; }
make install_sw > /tmp/openssl-install.log

# For the version, from the tags.
git config --global --add safe.directory /src
cmake -S /src -B /tmp/build -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_TOOLCHAIN_FILE=/src/cmake/arm-linux-gnueabihf.cmake \
    -DOPENSSL_ROOT_DIR=/opt/openssl -DEBISU_BUILD_SAMPLES=ON \
    -DEBISU_APP_ID=unused -DEBISU_DEFAULT_SITE=unused.invalid > /tmp/configure.log \
    || { cat /tmp/configure.log; exit 1; }
grep -E 'OpenSSL|EBISU_OPENSSL_USABLE' /tmp/configure.log || true
cmake --build /tmp/build -j"$(nproc)"
for target in ebisu_posix ebisu_openssl exampleapp; do
    if ! grep -q "^$target\$" <(cmake --build /tmp/build --target help | sed -n 's/^\.\.\. //p'); then
        echo "$target was not built" >&2
        exit 1
    fi
done

(cd /tmp/build && ctest -L small --output-on-failure)
# The OpenSSL port's tests, from the large tests' kii suite, against a server of
# their own on the loopback: the others need the live application, which the
# application ID and site given above are not.
(cd /src/tests/large_test/kii && qemu-arm -L /usr/arm-linux-gnueabihf \
    /tmp/build/tests/large_test/large_test_kii -# '[#openssl_socket_test]')

# Every ARM binary built, against the newest glibc symbol version it may need.
newest=""
failed=0
while IFS= read -r -d '' f; do
    case $(file -b "$f") in
        *'ELF 32-bit'*ARM*) ;;
        *) continue ;;
    esac
    v=$(arm-linux-gnueabihf-objdump -T "$f" | { grep -o 'GLIBC_[0-9.]*' || true; } | sed 's/GLIBC_//' | sort -u -V | tail -1)
    [ -n "$v" ] || continue
    newest=$(printf '%s\n%s\n' "$newest" "$v" | sed '/^$/d' | sort -V | tail -1)
    if [ "$(printf '%s\n%s\n' "$glibc_max" "$v" | sort -V | tail -1)" != "$glibc_max" ]; then
        echo "${f#/tmp/build/} needs GLIBC_$v, newer than GLIBC_$glibc_max" >&2
        failed=1
    fi
done < <(find /tmp/build -type f \( -name '*.so*' -o -perm -u+x \) -print0)
echo "newest glibc symbol version needed: GLIBC_$newest"
[ "$failed" = 0 ]

echo "stripped sizes, MinSizeRel, armv7-a hard float:"
for f in /tmp/build/core/libebisu_core.so /tmp/build/jkii/libebisu_jkii.so \
        /tmp/build/khc/libebisu_khc.so /tmp/build/kii/libebisu_kii.so \
        /tmp/build/tio/libebisu_tio.so /tmp/build/tio/linux-sample/exampleapp; do
    arm-linux-gnueabihf-strip -o /tmp/stripped "$(readlink -f "$f")"
    printf '  %-18s %8d bytes\n' "$(basename "$f")" "$(stat -c %s /tmp/stripped)"
done
