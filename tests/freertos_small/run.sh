#!/bin/sh
# Builds ebisu's small tests as firmware for QEMU's Cortex-M3 board and runs
# them, with CTest's JUnit report in test-results/junit, as CI does.
#
# Usage: tests/freertos_small/run.sh
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
build="$root/build/freertos_small"
cmake -S "$root/tests/freertos_small" -B "$build"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
# Verbose, for each suite's lines in the log, whether or not it passes.
ctest --test-dir "$build" --verbose \
    --output-junit "$root/test-results/junit/freertos_small.xml"
