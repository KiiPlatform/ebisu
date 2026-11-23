#!/bin/sh
# Checks where the version comes from, through CMake and through
# version/ebisu_version.sh, beyond what check_version_header.sh and
# check_archive_version.sh check:
#
# - in this checkout, what `git describe --tags` says;
# - in a copy inside another project's repository, not that project's tag;
# - version/VERSION, over either of those and over a source archive, picked up
#   by a build tree configured before it was written or changed, and refused
#   when it holds anything but a version.
#
# Usage: check_version_sources.sh <the header CMake generated for this checkout>
set -e
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
    echo "$*" >&2
    exit 1
}

# What a header defines a name as.
defined() {
    sed -n "s/^#define $2 //p" "$1"
}

expect() {
    got=$(defined "$1" "$2")
    if [ "$got" != "$3" ]; then
        fail "$1: $2 is $got, not $3"
    fi
}

# The script's header for the copy at $1 has to be what CMake generated.
same_as_cmake() {
    sh "$1/version/ebisu_version.sh" "$1/written.h" 2> "$work/script.log"
    if ! cmp -s "$1/build/generated/ebisu_version.h" "$1/written.h"; then
        echo "$1: version/ebisu_version.sh writes another header than CMake:" >&2
        diff "$1/build/generated/ebisu_version.h" "$1/written.h" >&2 || true
        exit 1
    fi
}

configure() {
    if ! cmake -S "$1" -B "$1/build" -DBUILD_TESTING=OFF > "$work/cmake.log" 2>&1; then
        cat "$work/cmake.log" >&2
        fail "$1: configuring failed"
    fi
    same_as_cmake "$1"
}

# Builds a target of the copy at $1, which configures it again if anything it
# was configured from has changed.
build() {
    if ! cmake --build "$1/build" --target ebisu_core > "$work/build.log" 2>&1; then
        cat "$work/build.log" >&2
        fail "$1: building failed"
    fi
    same_as_cmake "$1"
}

refused() {
    if cmake -S "$1" -B "$1/refused" -DBUILD_TESTING=OFF > "$work/cmake.log" 2>&1; then
        fail "$1: CMake took version/VERSION holding $2"
    fi
    grep -q "has to hold a version" "$work/cmake.log" ||
        fail "$1: CMake failed, but not over version/VERSION: $(cat "$work/cmake.log")"
    if sh "$1/version/ebisu_version.sh" "$1/written.h" 2> "$work/script.log"; then
        fail "$1: version/ebisu_version.sh took version/VERSION holding $2"
    fi
    grep -q "has to hold a version" "$work/script.log" ||
        fail "$1: version/ebisu_version.sh failed, but not over version/VERSION"
}

# Lets the next write change a file's and a directory's time from the last
# configure's, whatever the file system's resolution.
tick() {
    sleep 1
}

# This checkout.
expect "$1" EBISU_VERSION_FULL "\"$(git -C "$root" describe --tags)\""

# A copy inside another project's repository, which has a tag of its own. The
# copy is of the tree as it is committed, archive_version.txt unfilled.
project=$work/project
copy=$project/ebisu
mkdir -p "$copy"
git -C "$root" archive HEAD | tar -x -C "$copy"
git -C "$root" show HEAD:version/archive_version.txt > "$copy/version/archive_version.txt"
git -C "$project" init -q
git -C "$project" add -A
git -C "$project" -c user.name=ebisu -c user.email=ebisu@example.invalid \
    commit -q -m "ebisu, copied"
git -C "$project" tag v9.8.7
configure "$copy"
expect "$copy/written.h" EBISU_VERSION '"0.0.0"'
expect "$copy/written.h" EBISU_SOVERSION '"0.0"'
expect "$copy/written.h" EBISU_VERSION_FULL '"0.0.0"'
for log in "$work/cmake.log" "$work/script.log"; do
    grep -q "in another project's git repository" "$log" ||
        fail "$log: no warning that the tags are another project's"
done

# version/VERSION written into that copy, which its build tree picks up.
tick
echo v3.1.4-2-gabcdef0 > "$copy/version/VERSION"
build "$copy"
expect "$copy/written.h" EBISU_VERSION '"3.1.4"'
expect "$copy/written.h" EBISU_SOVERSION '"3.1"'
expect "$copy/written.h" EBISU_VERSION_FULL '"v3.1.4-2-gabcdef0"'

# And changed.
tick
echo 3.1.5 > "$copy/version/VERSION"
build "$copy"
expect "$copy/written.h" EBISU_VERSION '"3.1.5"'
expect "$copy/written.h" EBISU_VERSION_FULL '"3.1.5"'

# Anything but a version in it.
printf '3.1\n' > "$copy/version/VERSION"
refused "$copy" "3.1"
printf '3.1.4\n3.1.5\n' > "$copy/version/VERSION"
refused "$copy" "two versions"
printf '3.1.4 beta\n' > "$copy/version/VERSION"
refused "$copy" "a space"

# version/VERSION over a source archive's version, as a Windows editor writes
# it. The source's time is still the archive's, which is this checkout's.
unpacked=$work/archive
mkdir "$unpacked"
git -C "$root" archive HEAD | tar -x -C "$unpacked"
printf '3.0.0\r\n' > "$unpacked/version/VERSION"
configure "$unpacked"
expect "$unpacked/written.h" EBISU_VERSION '"3.0.0"'
expect "$unpacked/written.h" EBISU_SOVERSION '"3.0"'
expect "$unpacked/written.h" EBISU_VERSION_FULL '"3.0.0"'
expect "$unpacked/written.h" EBISU_SOURCE_TIME "$(defined "$1" EBISU_SOURCE_TIME)"
