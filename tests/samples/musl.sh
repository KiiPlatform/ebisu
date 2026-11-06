#!/bin/bash
# Builds ebisu for 32-bit x86 Linux on musl, in Alpine with its own OpenSSL:
# the libraries, platform's ports and the Linux sample. Runs the small tests and
# the OpenSSL port's. musl 1.2.0 made time_t 64 bits on 32-bit targets, so the
# two releases test both sides of that change.
#
# Usage: musl.sh <Alpine release>
#   3.12: musl 1.1.24, 32-bit time_t, OpenSSL 1.1.1
#   3.20: musl 1.2.5, 64-bit time_t, OpenSSL 3.3
# DOCKER names the container tool, docker by default; DOCKER_OPTS adds options
# to its run, such as --security-opt label=disable for podman with SELinux.
set -euo pipefail
release=$1
root=$(cd "$(dirname "$0")/../.." && pwd)

if [ "${MUSL_INSIDE:-}" != 1 ]; then
    # musl 1.1.24 makes every socket call through socketcall(2) on 32-bit x86,
    # which Docker 29's default seccomp profile refuses, with ENOSYS, as it
    # can't see the call's arguments: apk couldn't fetch, nor the tests open a
    # socket. musl 1.2.5 makes the calls directly, which the profile allows, so
    # it keeps the profile.
    seccomp=
    if [ "$release" = 3.12 ]; then
        seccomp="--security-opt seccomp=unconfined"
    fi
    # shellcheck disable=SC2086
    "${DOCKER:-docker}" run --rm ${DOCKER_OPTS:-} $seccomp --platform linux/386 -e MUSL_INSIDE=1 \
        -v "$root:/src:ro" "docker.io/library/alpine:$release" \
        sh -c 'apk add --no-cache bash > /dev/null && bash /src/tests/samples/musl.sh "$0"' "$release"
    exit
fi

apk add --no-cache build-base cmake openssl-dev linux-headers git > /dev/null
{ ldd 2>&1 || true; } | sed -n 1,2p
printf '#include <stdio.h>\n#include <time.h>\nint main(void) { printf("time_t is %%d bits\\n", (int)(8 * sizeof(time_t))); return 0; }\n' > /tmp/t.c
gcc /tmp/t.c -o /tmp/t && /tmp/t

# For the version, from the tags.
git config --global --add safe.directory /src
cmake -S /src -B /tmp/build -DCMAKE_BUILD_TYPE=Debug -DEBISU_BUILD_SAMPLES=ON \
    -DEBISU_APP_ID=unused -DEBISU_DEFAULT_SITE=unused.invalid > /tmp/configure.log \
    || { cat /tmp/configure.log; exit 1; }
grep -E 'Found OpenSSL|EBISU_OPENSSL_USABLE' /tmp/configure.log || true
cmake --build /tmp/build -j"$(nproc)"
for target in ebisu_posix ebisu_openssl ebisu_loggers exampleapp; do
    if ! grep -q "^$target\$" <(cmake --build /tmp/build --target help | sed -n 's/^\.\.\. //p'); then
        echo "$target was not built" >&2
        exit 1
    fi
done

(cd /tmp/build && ctest -L small --output-on-failure)
# The OpenSSL port's tests, from the large tests' kii suite, against a server of
# their own on the loopback: the others need the live application, which the
# application ID and site given above are not.
(cd /src/tests/large_test/kii && /tmp/build/tests/large_test/large_test_kii -# '[#openssl_socket_test]')
