#!/bin/bash
# Builds ebisu as each embedded sample's vendor build does: from the files the
# sample's Makefile copies into the vendor's tree, with that build's include
# paths, for the Cortex-M4 all three boards have, with arm-none-eabi-gcc. The
# objects are then linked whole against newlib, so that a source a build leaves
# out, or a header it cannot find, fails here rather than in the vendor's tools.
# Whole, and without --gc-sections: a reference from a function nothing calls
# would otherwise be dropped with it, unresolved or not.
#
# The samples' own files are not built: they need their vendors' SDKs, which
# cannot be downloaded without an account, or at all.
#
# Usage: cross_build.sh [gt202|wiced|cc3200]...   (all three by default)
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${CROSS_PREFIX:-arm-none-eabi-}
# As strict as CMake builds the libraries: ISO C99, warnings as errors.
cflags=(-mcpu=cortex-m4 -mthumb -std=c99 -pedantic-errors -Wall -Werror -Os)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# build <name> <include dirs, colon-separated> <sources...>
build() {
    local name=$1 incs=$2
    shift 2
    local out="$work/$name" args=() obj src
    mkdir -p "$out"
    IFS=: read -r -a dirs <<< "$incs"
    for d in "${dirs[@]}"; do
        args+=(-I "$d")
    done
    for src in "$@"; do
        obj="$out/$(basename "${src%.c}").o"
        "${prefix}gcc" "${cflags[@]}" "${args[@]}" -c "$src" -o "$obj"
    done
    "${prefix}ar" rcs "$out/libebisu.a" "$out"/*.o
    # The system calls newlib's stdio needs, which a board's support package
    # provides. Without them the linker takes nosys's, and warns that each of
    # those will always fail.
    cat > "$out/main.c" <<'EOF'
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, void* st) { (void)fd; (void)st; return -1; }
int _getpid(void) { return 1; }
int _isatty(int fd) { (void)fd; return 0; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int _lseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
int _read(int fd, char* buf, int len) { (void)fd; (void)buf; (void)len; return 0; }
int _write(int fd, const char* buf, int len) { (void)fd; (void)buf; return len; }
int main(void) { return 0; }
EOF
    "${prefix}gcc" "${cflags[@]}" -c "$out/main.c" -o "$out/main.o"
    "${prefix}gcc" -mcpu=cortex-m4 -mthumb --specs=nano.specs --specs=nosys.specs \
        -Wl,--fatal-warnings "$out/main.o" \
        -Wl,--whole-archive "$out/libebisu.a" -Wl,--no-whole-archive \
        -o "$out/$name.elf"
    echo "$name: $# sources, linked"
    "${prefix}size" "$out/libebisu.a" | awk 'NR > 1 { t += $1; d += $2; b += $3 }
        END { printf "  text %d, data %d, bss %d bytes\n", t, d, b }'
}

# Runs a sample's Makefile on a copy of the sources, so that the folder it fills
# in the sample's directory, where its settings get edited, is left alone. The
# copy has no git, so its ebisu_version.h has the version used without one.
copy() {
    local sample=$1 dir=$2 src="$work/src"
    if [ ! -d "$src" ]; then
        mkdir -p "$src"
        cp -r "$root"/jkii "$root"/khc "$root"/kii "$root"/tio "$root"/version "$src"
    fi
    make -s -C "$src/tio/$sample-sample" >/dev/null
    mv "$src/tio/$sample-sample/$dir" "$work/$sample"
}

gt202() {
    # Everything goes into custom/tio, which the IAR project builds whole.
    copy gt202 custom
    build gt202 "$work/gt202/tio" "$work"/gt202/tio/*.c
}

wiced() {
    # WICED builds the sources tio_iot.mk lists, and nothing else: those in
    # tio/ have to be exactly the ones the Makefile copies there.
    copy wiced tio_iot
    local dir="$work/wiced" listed copied
    listed=$(tr -d '\r' < "$dir/tio_iot.mk" | grep -oE '\./tio/[A-Za-z0-9_]+\.c' | sort)
    copied=$(cd "$dir" && ls ./tio/*.c | sort)
    if [ "$listed" != "$copied" ]; then
        echo "wiced: tio_iot.mk lists other sources than the Makefile copies:" >&2
        diff <(echo "$listed") <(echo "$copied") | sed 's/^</  listed only:/; s/^>/  copied only:/' | grep only >&2
        exit 1
    fi
    build wiced "$dir/tio" $(echo "$listed" | sed "s|^\./|$dir/|")
}

cc3200() {
    # Code Composer Studio builds the whole project, with these include paths.
    copy cc3200 freertos_ebisu_demo
    local d="$work/cc3200"
    build cc3200 "$d:$d/jkii/include:$d/jkii/libs/jsmn:$d/khc/include:$d/kii/include:$d/tio/include" \
        "$d"/jkii/src/*.c "$d"/jkii/libs/jsmn/*.c "$d"/khc/src/*.c "$d"/kii/*.c "$d"/tio/*.c
}

samples=("$@")
if [ ${#samples[@]} -eq 0 ]; then
    samples=(gt202 wiced cc3200)
fi
for s in "${samples[@]}"; do
    case $s in
        gt202|wiced|cc3200) "$s" ;;
        *) echo "unknown sample: $s" >&2; exit 2 ;;
    esac
done
