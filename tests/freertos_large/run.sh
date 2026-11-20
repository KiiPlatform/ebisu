#!/bin/sh
# Builds ebisu's large tests as firmware for QEMU's Cortex-M3 board and runs
# them against the live Kii application, each suite retried once, as the
# host's large tests are, with CTest's JUnit report in test-results/junit, as
# CI does.
#
# Usage: tests/freertos_large/run.sh [<kernel> <lwip> [polarssl]]
#   with APP_ID, DEFAULT_SITE, CLIENT_ID and CLIENT_SECRET in the environment.
#   Without arguments, FreeRTOS-Plus-TCP and mbedTLS 3.6.7 on V11.3.3; with
#   them, lwIP and mbedTLS 2.4.0, as Realtek's SDKs configure them: V8.1.2
#   1.4.1, the AmebaZ's, or V10.0.1 2.0.2, the AmebaZ2's; and with polarssl,
#   PolarSSL 1.3.8 in place of mbedTLS, as the SDK for AmebaZ configures it
#   (see README.md).
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
kernel=${1:-V11.3.3}
lwip=${2:-}
tls=${3:-mbedtls}
if [ -n "$lwip" ] && [ "$tls" != mbedtls ]; then
    name="freertos_large-$kernel-lwip-$lwip-$tls"
elif [ -n "$lwip" ]; then
    name="freertos_large-$kernel-lwip-$lwip"
else
    name=freertos_large
fi
build="$root/build/$name"
: "${APP_ID:?}" "${DEFAULT_SITE:?}" "${CLIENT_ID:?}" "${CLIENT_SECRET:?}"
mkdir -p "$build"
# The admin's credentials, for the firmware to read through semihosting: in a
# file only this user can read, removed when the run ends.
env_file="$build/ltest.env"
trap 'rm -f "$env_file"' EXIT INT TERM
(
    umask 077
    printf 'CLIENT_ID=%s\nCLIENT_SECRET=%s\n' "$CLIENT_ID" "$CLIENT_SECRET" > "$env_file"
    # CI's, for the tests to fail rather than skip without the credentials.
    if [ -n "${CI:-}" ]; then
        printf 'CI=%s\n' "$CI" >> "$env_file"
    fi
)
# The application's ID and site from the environment, not a configure's
# before: -U drops what the cache has, for CMakeLists.txt to take them anew,
# and keeps them off the command line.
cmake -S "$root/tests/freertos_large" -B "$build" -U EBISU_APP_ID -U EBISU_DEFAULT_SITE \
    -D EBISU_LTEST_ENV_FILE="$env_file" -D EBISU_LTEST_KERNEL="$kernel" -D EBISU_LTEST_LWIP="$lwip" \
    -D EBISU_LTEST_TLS="$tls"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
# A suite's output only when it fails, as the host's large preset has it: what
# the tests print, the responses' headers among it, stays out of the logs
# otherwise.
ctest --test-dir "$build" --output-on-failure --no-tests=error --repeat until-pass:2 \
    --output-junit "$root/test-results/junit/$name.xml"
