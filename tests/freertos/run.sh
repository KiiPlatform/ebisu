#!/bin/sh
# Builds the FreeRTOS port's tests for a kernel and a configuration, and runs
# them in QEMU, with CTest's JUnit report in test-results/junit, as CI does.
#
# Usage: tests/freertos/run.sh <kernel> <configuration>
#   kernel: V10.0.1, V11.1.0 or V11.3.3; configuration: a, b or c (see README.md)
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
kernel=$1
config=$2
build="$root/build/freertos-$kernel-$config"
cmake -S "$root/tests/freertos" -B "$build" \
    -D EBISU_TEST_KERNEL="$kernel" -D EBISU_TEST_CONFIG="$config"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
# Verbose, for the firmware's own lines, its checks and how many failed, in the
# log, whether or not they pass.
ctest --test-dir "$build" --verbose \
    --output-junit "$root/test-results/junit/freertos-$kernel-$config.xml"
