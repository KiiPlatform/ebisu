#!/bin/sh
# Checks that version/ebisu_version.sh, which the embedded samples' Makefiles
# run, writes the ebisu_version.h CMake writes.
#
# Usage: check_version_header.sh <the header CMake generated>
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
written=$(mktemp)
trap 'rm -f "$written"' EXIT
sh "$root/version/ebisu_version.sh" "$written"
if ! cmp -s "$1" "$written"; then
    echo "version/ebisu_version.sh writes another ebisu_version.h than CMake:" >&2
    diff "$1" "$written" >&2 || true
    exit 1
fi
