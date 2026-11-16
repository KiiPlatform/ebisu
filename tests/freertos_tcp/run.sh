#!/bin/sh
# Builds the FreeRTOS-Plus-TCP socket's tests for a kernel and a
# FreeRTOS-Plus-TCP, and runs them in QEMU against run_tcp_test.py's servers,
# with CTest's JUnit report in test-results/junit, as CI does.
#
# Usage: tests/freertos_tcp/run.sh <kernel> <freertos-plus-tcp> [ipv6]
#   kernel: V11.1.0 or V11.3.3; freertos-plus-tcp: V4.2.6 or V4.4.2; ipv6 for
#   FreeRTOS-Plus-TCP with IPv6 too
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
kernel=$1
tcp=$2
ipv6=OFF
name="$kernel-$tcp"
if [ "${3:-}" = ipv6 ]; then
    ipv6=ON
    name="$name-ipv6"
fi
build="$root/build/freertos_tcp-$name"
cmake -S "$root/tests/freertos_tcp" -B "$build" \
    -D EBISU_TEST_KERNEL="$kernel" -D EBISU_TEST_PLUS_TCP="$tcp" -D EBISU_TEST_IPV6="$ipv6"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
# Verbose, for the firmware's own lines, its checks and how many failed, in the
# log, whether or not they pass.
ctest --test-dir "$build" --verbose \
    --output-junit "$root/test-results/junit/freertos_tcp-$name.xml"
