#!/bin/sh
# Writes ebisu_version.h for a build that does not run CMake, as
# version/EbisuVersion.cmake does for one that does: the embedded samples, which
# their vendors' tools build from the files the samples' Makefiles copy. A CTest
# test checks that the two write the same header.
#
# Usage: ebisu_version.sh <output>
set -e
if [ $# -ne 1 ]; then
    echo "usage: $0 <output>" >&2
    exit 2
fi
dir=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$dir")

# The numeric version is the first vX.Y.Z in `git describe --tags`, without the
# v, or 0.0.0; the soversion is its major; the full version is `git describe`
# output as it is, or the numeric version without git.
describe=$(git -C "$root" describe --tags 2>/dev/null || true)
version=$(printf '%s\n' "$describe" | grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+' | head -n 1 | sed 's/^v//')
if [ -z "$version" ]; then
    version=0.0.0
fi
soversion=${version%%.*}
version_full=${describe:-$version}

# When the source was committed, or the same fixed date as CMake's without git.
source_time=$(git -C "$root" log -1 --format=%ct 2>/dev/null || true)
case $source_time in
    ''|*[!0-9]*) source_time=1793059200 ;; # 2026-10-27T00:00:00Z
esac

sed -e "s|@version@|$version|g" \
    -e "s|@soversion@|$soversion|g" \
    -e "s|@version_full@|$version_full|g" \
    -e "s|@source_time@|$source_time|g" \
    "$dir/ebisu_version.h.in" > "$1"
