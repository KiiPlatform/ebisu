#!/bin/sh
# Writes ebisu_version.h for a build that does not run CMake, as
# version/EbisuVersion.cmake does for one that does: the embedded samples, which
# their vendors' tools build from the files the samples' Makefiles copy. The
# samples job in CI checks that the two write the same header.
#
# Usage: ebisu_version.sh <output>
set -e
if [ $# -ne 1 ]; then
    echo "usage: $0 <output>" >&2
    exit 2
fi
dir=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$dir")

# A source archive made by git has no repository to ask, and says what it would
# have, as in EbisuVersion.cmake.
archive=$dir/archive_version.txt
archive_time=
if grep -qs '^time=[0-9]' "$archive"; then
    archive_describe=$(sed -n 's/^describe=//p' "$archive")
    archive_time=$(sed -n 's/^time=//p' "$archive")
fi

# The version comes from the first there is of version/VERSION, a source
# archive and `git describe --tags` in ebisu's own repository, as in
# EbisuVersion.cmake, which says why.
version_file=$dir/VERSION
version=
why=
if [ -f "$version_file" ]; then
    describe=$(sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e '/^$/d' \
        "$version_file")
    if [ "$(printf '%s\n' "$describe" | sed -n '$=')" != 1 ] ||
        ! printf '%s\n' "$describe" |
            grep -Eqx 'v?[0-9]+\.[0-9]+\.[0-9]+[-+.0-9A-Za-z]*'; then
        echo "$0: $version_file has to hold a version and nothing else," \
            "as 3.0.0 or v3.0.0-rc.1, not: $describe" >&2
        exit 1
    fi
    version=$(printf '%s\n' "${describe#v}" | grep -oE '^[0-9]+\.[0-9]+\.[0-9]+')
else
    if [ -n "$archive_time" ]; then
        describe=$archive_describe
        described_by="the source archive"
        why="the source archive is of a commit no tag comes before"
    elif ! cdup=$(git -C "$root" rev-parse --show-cdup 2>/dev/null); then
        describe=
        why="ebisu is in neither a git repository nor a source archive made by git"
    elif [ -n "$cdup" ]; then
        describe=
        why="ebisu is in another project's git repository, whose tags aren't ebisu's"
    else
        describe=$(git -C "$root" describe --tags) || describe=
        described_by=git
        why="\`git describe --tags\` failed, as it does in a clone made without the tags"
    fi
    # The numeric version is the first vX.Y.Z in what describes the source,
    # without the v.
    version=$(printf '%s\n' "$describe" | grep -oE 'v[0-9]+\.[0-9]+\.[0-9]+' | head -n 1 | sed 's/^v//')
    if [ -z "$version" ] && [ -n "$describe" ]; then
        why="$described_by describes it as $describe, which holds no vX.Y.Z"
    fi
fi
if [ -z "$version" ]; then
    echo "$0: warning: $why. ebisu builds as 0.0.0, unless $version_file" \
        "says its version, as 3.0.0." >&2
    version=0.0.0
fi

# The soversion is the numeric version's major and minor; the full version is
# what describes the source as it is, or the numeric version without any.
soversion=${version%.*}
version_full=${describe:-$version}

# When the source was committed: as the source archive says, or as git does,
# for the repository ebisu is in, as in EbisuVersion.cmake.
if [ -n "$archive_time" ]; then
    source_time=$archive_time
else
    source_time=$(git -C "$root" log -1 --format=%ct 2>/dev/null || true)
fi

# Or the same fixed date as CMake's without either.
case $source_time in
    ''|*[!0-9]*) source_time=1793059200 ;; # 2026-10-27T00:00:00Z
esac

sed -e "s|@version@|$version|g" \
    -e "s|@soversion@|$soversion|g" \
    -e "s|@version_full@|$version_full|g" \
    -e "s|@source_time@|$source_time|g" \
    "$dir/ebisu_version.h.in" > "$1"
