#!/bin/bash
# Checks the versioning policy README.md's Packaging section states: X.Y.Z
# keeps the ABI of X.Y.(Z-1), and X.Y the API of X.(Y-1).
#
# - ABI: the shared libraries of each release of this tree's X.Y, vX.Y.Z, are
#   compared with this tree's by abidiff, over the public headers. Functions
#   may be added, and enumerators at the end of an enum, which abidiff takes
#   for harmless; any other change fails.
# - API: the Linux sample of the newest release of an earlier minor version
#   of this tree's major, which uses the public API alone, has to build
#   against this tree's installed copy.
#
# Neither applies before there is such a release, as on master while it is
# becoming X.0. Either way, the check then checks itself: against copies of
# this tree, one with a function added has to pass, and one with a field
# added to a public struct, or a public declaration taken out, has to fail.
#
# Needs the tags, CMake, a C compiler, pkg-config, OpenSSL's headers and
# abidiff, from libabigail.
#
# Usage: compat_check.sh
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
failed=0

# The version this tree describes itself as, X.Y.Z.
sh "$root/version/ebisu_version.sh" "$work/version.h"
version=$(sed -n 's/^#define EBISU_VERSION "\(.*\)"$/\1/p' "$work/version.h")
major=${version%%.*}
minor=${version#*.}
minor=${minor%%.*}
echo "This tree: $version ($(sed -n 's/^#define EBISU_VERSION_FULL "\(.*\)"$/\1/p' "$work/version.h"))"

# The releases: vX.Y.Z, without a pre-release's suffix.
releases=$(git -C "$root" tag --list 'v[0-9]*' | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | sort -V || true)
this_commit=$(git -C "$root" rev-parse HEAD)

# Unpacks <ref> into <dir> as a source archive, which says its version.
unpack() {
    mkdir -p "$2"
    git -C "$root" archive "$1" | tar -x -C "$2"
}

# Builds the source tree <dir>'s shared libraries with debug information, for
# abidiff, and installs them into <dir>/prefix. Warnings don't stop an older
# tree's build.
build() {
    if ! { cmake -S "$1" -B "$1/build" -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON \
                -DCMAKE_BUILD_TYPE=Debug -DEBISU_WERROR=OFF &&
            cmake --build "$1/build" -j"$(nproc)" &&
            cmake --install "$1/build" --prefix "$1/prefix"; } > "$1.log" 2>&1; then
        cat "$1.log"
        echo "building $1 failed" >&2
        exit 1
    fi
}

# Whether the installed copy <new> keeps the ABI of the installed copy <old>.
abi_kept() {
    local kept=0 lib old new
    for lib in core jkii khc kii tio; do
        old=$(find "$1" -name "libebisu_$lib.so.*.*.*" -type f)
        new=$(find "$2" -name "libebisu_$lib.so.*.*.*" -type f)
        if ! abidiff --no-added-syms --headers-dir1 "$1/include" --headers-dir2 "$2/include" \
                "$old" "$new" > "$work/abidiff.txt" 2>&1; then
            echo "libebisu_$lib:"
            cat "$work/abidiff.txt"
            kept=1
        fi
    done
    return $kept
}

# Whether the Linux sample of the source tree <src> builds against the
# installed copy <prefix>. A call of what the headers don't declare, or with
# arguments of types they don't take, is an error, as C99 and C11 have it
# but GCC 13 doesn't. Ports first: they call into the libraries.
api_kept() {
    local pc flags
    pc=$(dirname "$(find "$2" -name 'ebisu-tio.pc')")
    flags=$(PKG_CONFIG_PATH=$pc pkg-config --cflags --libs \
        ebisu-openssl ebisu-posix ebisu-loggers ebisu-tio) || return 1
    # shellcheck disable=SC2086 # the flags are words
    if ! cc -std=gnu11 -Werror=implicit-function-declaration \
            -Werror=incompatible-pointer-types -Werror=int-conversion \
            -o "$work/exampleapp" "$1"/tio/linux-sample/*.c -I"$1/tio/linux-sample" \
            $flags > "$work/api.txt" 2>&1; then
        cat "$work/api.txt"
        return 1
    fi
}

unpack HEAD "$work/this"
build "$work/this"

# ABI: each release of this X.Y, but one this tree is.
compared=0
for tag in $(printf '%s\n' "$releases" | grep -E "^v$major\.$minor\.[0-9]+$" || true); do
    [ "$(git -C "$root" rev-parse "$tag^{commit}")" = "$this_commit" ] && continue
    compared=1
    unpack "$tag" "$work/$tag"
    build "$work/$tag"
    if abi_kept "$work/$tag/prefix" "$work/this/prefix"; then
        echo "ABI of $tag: kept"
    else
        echo "ABI of $tag: changed, which $major.$minor.x doesn't allow" >&2
        failed=1
    fi
done
[ $compared = 1 ] || echo "ABI: no release of $major.$minor before this to compare with"

# API: the newest release of an earlier minor version of this major.
api_tag=$(printf '%s\n' "$releases" | grep -E "^v$major\.[0-9]+\.[0-9]+$" |
    awk -F. -v m="$minor" '$2 < m' | tail -n 1 || true)
if [ -n "$api_tag" ]; then
    unpack "$api_tag" "$work/$api_tag"
    if api_kept "$work/$api_tag" "$work/this/prefix"; then
        echo "API of $api_tag: kept"
    else
        echo "API of $api_tag: broken, which $major.$minor doesn't allow" >&2
        failed=1
    fi
else
    echo "API: no release of $major.x before $major.$minor to compare with"
fi

# The check itself, on copies of this tree. A function added keeps the ABI.
unpack HEAD "$work/added"
sed -i '/^#pragma GCC visibility push(default)$/{n;a int kii_compat_check_added(void);
}' "$work/added/kii/include/kii.h"
if ! grep -q '^int kii_compat_check_added(void);$' "$work/added/kii/include/kii.h"; then
    echo "check: no function was declared in the copy's kii.h" >&2
    exit 1
fi
echo 'int kii_compat_check_added(void) { return 0; }' >> "$work/added/kii/kii.c"
build "$work/added"
if ! nm -D --defined-only "$(find "$work/added/prefix" -name 'libebisu_kii.so.*.*.*' -type f)" |
        grep -q ' kii_compat_check_added$'; then
    echo "check: the function added to a copy isn't exported" >&2
    failed=1
elif ! abi_kept "$work/this/prefix" "$work/added/prefix" > "$work/self.txt"; then
    cat "$work/self.txt"
    echo "check: a function added counts as an ABI change" >&2
    failed=1
fi
# A field added to a public struct changes it.
unpack HEAD "$work/field"
sed -i '/^typedef struct kii_author_t$/{n;a \    char _compat_check;
}' "$work/field/kii/include/kii.h"
if ! grep -q '_compat_check;' "$work/field/kii/include/kii.h"; then
    echo "check: no field was added to the copy's kii_author_t" >&2
    exit 1
fi
build "$work/field"
if abi_kept "$work/this/prefix" "$work/field/prefix" > /dev/null; then
    echo "check: a field added to kii_author_t goes unnoticed" >&2
    failed=1
fi
# This tree's sample builds against its own copy, and not against one without
# a declaration it uses.
if ! api_kept "$work/this" "$work/this/prefix"; then
    echo "check: this tree's sample doesn't build against this tree" >&2
    failed=1
fi
cp -r "$work/this/prefix" "$work/undeclared"
sed -i '/^tio_code_t tio_handler_start($/,/);$/d' "$work/undeclared/include/tio.h"
if grep -q '^tio_code_t tio_handler_start($' "$work/undeclared/include/tio.h"; then
    echo "check: tio_handler_start() is still declared in the copy" >&2
    exit 1
fi
if api_kept "$work/this" "$work/undeclared" > /dev/null; then
    echo "check: the sample builds without tio_handler_start() declared" >&2
    failed=1
fi
[ $failed = 1 ] || echo "check: what it has to notice, it does"
exit $failed
