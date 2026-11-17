#!/bin/sh
# Builds ebisu's large tests as firmware for QEMU's Cortex-M3 board and runs
# them against the live Kii application, each suite retried once, as the
# host's large tests are, with CTest's JUnit report in test-results/junit, as
# CI does.
#
# Usage: tests/freertos_large/run.sh
#   with APP_ID, DEFAULT_SITE, CLIENT_ID and CLIENT_SECRET in the environment.
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build/freertos_large"
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
    -D EBISU_LTEST_ENV_FILE="$env_file"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
# A suite's output only when it fails, as the host's large preset has it: what
# the tests print, the responses' headers among it, stays out of the logs
# otherwise.
ctest --test-dir "$build" --output-on-failure --no-tests=error --repeat until-pass:2 \
    --output-junit "$root/test-results/junit/freertos_large.xml"
