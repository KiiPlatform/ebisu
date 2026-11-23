#!/bin/bash
# Checks that a tree keeps the ABI of every release of its major.minor.
#
#   tests/compat_check.sh [CANDIDATE]
#
# CANDIDATE is a git ref of the repository this runs in, HEAD by default. It
# and each vX.Y.Z tag of its X.Y are built as shared libraries with debug
# information, and abidiff compares each release's libkhc, libjkii, libkii and
# libtio with the candidate's, over the public headers. Under the versioning
# policy X.Y.Z keeps the ABI of X.Y.(Z-1), so anything abidiff reports, past
# what it filters as harmless, fails: a function or a variable removed or
# changed, a type of another size or layout, a soname changed. What is only
# added passes, functions and enumerators appended to an enum, as an
# application built against the release still runs with the candidate.
#
# A tree whose X.Y has no release yet has nothing to keep, and passes.
#
# Needs cmake, ninja, git, readelf and abidiff (libabigail). Builds
# into $COMPAT_WORK, by default ${RUNNER_TEMP:-/tmp}/ebisu-compat, and writes
# a report to $GITHUB_STEP_SUMMARY when it is set.
set -euo pipefail

candidate=${1:-HEAD}
repo=$(git rev-parse --show-toplevel)
work=${COMPAT_WORK:-${RUNNER_TEMP:-/tmp}/ebisu-compat}
summary=${GITHUB_STEP_SUMMARY:-/dev/null}
libraries="khc jkii kii tio"
headers="khc/include jkii/include jkii/libs/jsmn kii/include tio/include"

rm -rf "$work"; mkdir -p "$work"

# Builds REF's libraries into $work/NAME.
build() {
    local ref=$1 name=$2
    # A clone, with the repository's tags, so that the version, and the soname
    # from it, are what git describes for REF, as in REF's own build.
    git clone -q --shared --no-checkout "$repo" "$work/$name/src"
    git -C "$work/$name/src" checkout -q --detach "$(git -C "$repo" rev-parse "$ref^{commit}")"
    # abidiff reads the types from the debug information. The tests aren't
    # built: nothing here runs them. A release's warnings under a newer
    # compiler don't matter here.
    cmake -S "$work/$name/src" -B "$work/$name/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="-g -Og" \
        -DBUILD_SHARED_LIBS=ON -DEBISU_WERROR=OFF \
        > "$work/$name/configure.log" 2>&1 \
        || { cat "$work/$name/configure.log"; return 1; }
    # shellcheck disable=SC2086
    ninja -C "$work/$name/build" $libraries > "$work/$name/build.log" 2>&1 \
        || { tail -50 "$work/$name/build.log"; return 1; }
}

# The file of library LIB in build NAME, not its links.
library() {
    find "$work/$2/build" -name "lib$1.so*" -type f | sort | tail -1
}

soname() {
    readelf -d "$1" | sed -n 's/.*Library soname: \[\(.*\)\].*/\1/p'
}

build "$candidate" candidate
version=$(sed -n 's/^#define EBISU_VERSION "\(.*\)"$/\1/p' \
    "$(find "$work/candidate/build" -name ebisu_version.h | head -1)")
major_minor=$(echo "$version" | grep -o -E '^[0-9]+\.[0-9]+')
releases=$(git -C "$repo" tag -l "v$major_minor.*" \
    | grep -E "^v${major_minor//./\\.}\.[0-9]+$" | sort -V || true)

{
    echo "## ABI of \`$candidate\` ($version) against the releases of $major_minor"
    echo
} >> "$summary"
if [ -z "$releases" ]; then
    echo "No release of $major_minor yet: nothing to compare." | tee -a "$summary"
    exit 0
fi

failed=0
echo "| release | library | soname | ABI |" >> "$summary"
echo "|---|---|---|---|" >> "$summary"
for tag in $releases; do
    build "$tag" "$tag"
    header_args=()
    for h in $headers; do
        header_args+=(--headers-dir1 "$work/$tag/src/$h" --headers-dir2 "$work/candidate/src/$h")
    done
    for lib in $libraries; do
        old=$(library "$lib" "$tag"); new=$(library "$lib" candidate)
        if [ -z "$old" ] || [ -z "$new" ]; then
            echo "| $tag | lib$lib | | **not built** |" >> "$summary"; failed=1; continue
        fi
        old_soname=$(soname "$old"); new_soname=$(soname "$new")
        soname_note=$new_soname
        if [ "$old_soname" != "$new_soname" ]; then
            soname_note="**$old_soname -> $new_soname**"; failed=1
        fi
        report="$work/$tag-lib$lib.abidiff"
        set +e
        abidiff --no-added-syms "${header_args[@]}" "$old" "$new" > "$report" 2>&1
        status=$?
        set -e
        echo "::group::abidiff $tag lib$lib (exit $status)"; cat "$report"; echo "::endgroup::"
        # Bits: 1 an error, 2 a usage error, 4 an ABI change, 8 an
        # incompatible one.
        if [ $status -eq 0 ]; then
            echo "| $tag | lib$lib | $soname_note | kept |" >> "$summary"
        else
            echo "| $tag | lib$lib | $soname_note | **changed** (abidiff exit $status) |" >> "$summary"
            failed=1
        fi
    done
done

if [ $failed -ne 0 ]; then
    {
        echo
        echo "<details><summary>abidiff reports</summary>"
        echo
        echo '```'
        cat "$work"/*.abidiff
        echo '```'
        echo "</details>"
    } >> "$summary"
    echo "The ABI of a release of $major_minor isn't kept: see abidiff's reports above." >&2
fi
exit $failed
