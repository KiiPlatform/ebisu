#!/bin/sh
# Checks that a source archive, which has no repository to ask, reports the
# version of the commit it was made from, through CMake and through
# version/ebisu_version.sh. Compares both with the header of this checkout,
# which is why HEAD has to be what that header was generated from.
#
# Usage: check_archive_version.sh <the header CMake generated>
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
unpacked=$(mktemp -d)
trap 'rm -rf "$unpacked"' EXIT
git -C "$root" archive HEAD | tar -x -C "$unpacked"
cmake -S "$unpacked" -B "$unpacked/build" -DBUILD_TESTING=OFF > /dev/null
sh "$unpacked/version/ebisu_version.sh" "$unpacked/written.h"
for header in "$unpacked/build/generated/ebisu_version.h" "$unpacked/written.h"; do
    if ! cmp -s "$1" "$header"; then
        echo "A source archive reports another version than this checkout:" >&2
        diff "$1" "$header" >&2 || true
        exit 1
    fi
done
