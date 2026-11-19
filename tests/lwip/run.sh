#!/bin/sh
# Builds the lwIP port's tests for an lwIP and a configuration, and runs
# them, with CTest's JUnit report in test-results/junit, as CI does.
#
# Usage: tests/lwip/run.sh <lwip> <configuration> [memcheck]
#   lwip: 2.0.2, 2.1.3 or 2.2.1; configuration: a or b (see README.md);
#   memcheck runs them under valgrind.
set -eu
root=$(cd "$(dirname "$0")/../.." && pwd)
lwip=$1
config=$2
build="$root/build/lwip-$lwip-$config"
cmake -S "$root/tests/lwip" -B "$build" \
    -D EBISU_TEST_LWIP="$lwip" -D EBISU_TEST_CONFIG="$config"
cmake --build "$build" -j "$(nproc)"
mkdir -p "$root/test-results/junit"
if [ "${3:-}" = memcheck ]; then
    # As ebisu's own CTest runs valgrind: a definite leak or an error fails.
    valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=1 \
        "$build/lwip_test" -r junit -o "$root/test-results/junit/lwip-$lwip-$config-memcheck.xml"
else
    ctest --test-dir "$build" --output-on-failure \
        --output-junit "$root/test-results/junit/lwip-$lwip-$config.xml"
fi
