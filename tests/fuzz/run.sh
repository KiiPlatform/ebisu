#!/bin/sh
# Runs each fuzz target from build/fuzz for the given number of seconds, from
# its corpus in the repository. What a run adds goes to a temporary directory,
# so the corpus only grows by hand. A finding is written to the working
# directory as crash-*, timeout-* or similar, and fails the script.
set -e

seconds=${1:-60}
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for name in khc_response jkii_parse mqtt_task tio_command; do
    echo "== $name"
    mkdir "$work/$name"
    dict=""
    if [ "$name" = khc_response ]; then
        dict="-dict=$root/tests/fuzz/http.dict"
    fi
    "$root/build/fuzz/tests/fuzz/fuzz_$name" $dict \
        -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048 -print_final_stats=1 \
        "$work/$name" "$root/tests/fuzz/corpus/$name"
done
