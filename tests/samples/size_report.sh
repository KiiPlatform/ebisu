#!/bin/bash
# Reports how large ebisu's code is, built as usual and with EBISU_MIN_SIZE:
#
# - the host's shared libraries, built Release, MinSizeRel, and MinSizeRel
#   with EBISU_MIN_SIZE;
# - a program for a Cortex-M3 without an operating system, a thing that
#   onboards and runs tio's handler and updater, linked with --gc-sections
#   against the installed static libraries, built MinSizeRel, with
#   EBISU_MIN_SIZE, with link-time optimization as well, and with
#   EBISU_MIN_SIZE and without strtod(), EBISU_STRTOD=OFF.
#
# Text is code and constant data, what takes flash; data and bss take RAM.
# Nothing is checked: the sizes are for reading, in the log and, in GitHub
# Actions, in the job's summary.
#
# Usage: size_report.sh   (the Cortex-M3 part needs arm-none-eabi-gcc)
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
report="$work/report.md"

# configure_build <dir> <cmake arguments...>
configure_build() {
    local dir=$1
    shift
    cmake -S "$root" -B "$dir" -DBUILD_TESTING=OFF "$@" > "$dir.log" 2>&1 ||
        { cat "$dir.log" >&2; exit 1; }
    cmake --build "$dir" -j "$(nproc)" >> "$dir.log" 2>&1 || { cat "$dir.log" >&2; exit 1; }
}

# The text of the named files, as `size` counts it.
text() {
    size "$@" | awk 'NR > 1 { print $1 }'
}

{
    echo "### Shared libraries, $(uname -m), $(cc --version | head -1)"
    echo
    echo "Text, in bytes."
    echo
    echo "| Library | Release | MinSizeRel | MinSizeRel, EBISU_MIN_SIZE |"
    echo "|---|---|---|---|"
} >> "$report"
configure_build "$work/release" -DCMAKE_BUILD_TYPE=Release
configure_build "$work/minsizerel" -DCMAKE_BUILD_TYPE=MinSizeRel
configure_build "$work/min" -DCMAKE_BUILD_TYPE=MinSizeRel -DEBISU_MIN_SIZE=ON
for lib in core jkii khc kii tio; do
    row="| libebisu_$lib.so"
    for b in release minsizerel min; do
        row="$row | $(text "$(find "$work/$b/$lib" -maxdepth 1 -name "libebisu_$lib.so.*.*.*" -type f)")"
    done
    echo "$row |" >> "$report"
done

if command -v arm-none-eabi-gcc > /dev/null; then
    # A thing as the samples are: it onboards, then runs the handler, which
    # takes commands, and the updater, which uploads its state, with buffers of
    # its own and the samples' pool for the request headers, so that ebisu takes
    # nothing from the heap. It is linked, never run, so the socket and the
    # system are left out.
    cat > "$work/thing.c" <<'EOF'
#include <tio.h>

#include "header_pool.h"

static size_t state_size(void* userdata) { (void)userdata; return 2; }
static size_t read_state(char* buffer, size_t size, void* userdata)
{
    (void)userdata;
    if (size < 2) {
        return 0;
    }
    buffer[0] = '{';
    buffer[1] = '}';
    return 2;
}
static bool action(tio_action_t* a, tio_action_err_t* err,
        tio_action_result_data_t* data, void* userdata)
{
    (void)a; (void)err; (void)data; (void)userdata;
    return true;
}

static char http_buff[2048];
static char mqtt_buff[1024];
static char stream_buff[1024];
static char resp_header_buff[256];
static header_pool_t headers;
static char updater_buff[1024];
static char updater_stream_buff[1024];
static char updater_resp_header_buff[256];
static header_pool_t updater_headers;
static tio_handler_t handler;
static tio_updater_t updater;

int main(void)
{
    tio_author_t author;
    tio_handler_init(&handler);
    tio_handler_set_app(&handler, "app", "api.example.com");
    tio_handler_set_http_buff(&handler, http_buff, sizeof(http_buff));
    tio_handler_set_mqtt_buff(&handler, mqtt_buff, sizeof(mqtt_buff));
    tio_handler_set_stream_buff(&handler, stream_buff, sizeof(stream_buff));
    tio_handler_set_resp_header_buff(&handler, resp_header_buff, sizeof(resp_header_buff));
    header_pool_init(&headers);
    tio_handler_set_cb_slist_resource(&handler, header_pool_alloc, header_pool_free,
            &headers, &headers);
    tio_handler_set_keep_alive_interval(&handler, 300);
    if (tio_handler_onboard(&handler, "vid", "pass", NULL, NULL, NULL, NULL) != TIO_ERR_OK) {
        return 1;
    }
    tio_handler_copy_author(&handler, &author);
    tio_handler_start(&handler, &author, action, NULL);
    tio_updater_init(&updater);
    tio_updater_set_app(&updater, "app", "api.example.com");
    tio_updater_set_buff(&updater, updater_buff, sizeof(updater_buff));
    tio_updater_set_stream_buff(&updater, updater_stream_buff, sizeof(updater_stream_buff));
    tio_updater_set_resp_header_buff(&updater, updater_resp_header_buff,
            sizeof(updater_resp_header_buff));
    header_pool_init(&updater_headers);
    tio_updater_set_cb_slist_resource(&updater, header_pool_alloc, header_pool_free,
            &updater_headers, &updater_headers);
    tio_updater_set_interval(&updater, 60);
    tio_updater_start(&updater, &author, state_size, NULL, read_state, NULL);
    return 0;
}

/* The system calls newlib's stdio needs, which a board's support package
 * provides. */
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, void* st) { (void)fd; (void)st; return -1; }
int _getpid(void) { return 1; }
int _isatty(int fd) { (void)fd; return 0; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int _lseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return -1; }
int _read(int fd, char* buf, int len) { (void)fd; (void)buf; (void)len; return 0; }
int _write(int fd, const char* buf, int len) { (void)fd; (void)buf; return len; }
EOF
    arm=(-mcpu=cortex-m3 -mthumb)
    # thing <name> <flags for the thing's compile and link...> -- <cmake arguments...>
    thing() {
        local name=$1 flags=()
        shift
        while [ "$1" != "--" ]; do
            flags+=("$1")
            shift
        done
        shift
        configure_build "$work/$name" -DCMAKE_BUILD_TYPE=MinSizeRel \
            -DCMAKE_TOOLCHAIN_FILE="$root/tio/freertos-sample/arm-none-eabi.cmake" \
            -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="$work/$name/prefix" "$@"
        cmake --install "$work/$name" >> "$work/$name.log" 2>&1
        local src
        for src in "$work/thing.c" "$root/tio/freertos-sample/header_pool.c"; do
            arm-none-eabi-gcc "${arm[@]}" -Os "${flags[@]}" -std=c99 -Wall -Werror \
                -I "$work/$name/prefix/include" -I "$root/tio/freertos-sample" \
                -c "$src" -o "$work/$name/$(basename "$src" .c).o"
        done
        arm-none-eabi-gcc "${arm[@]}" -Os "${flags[@]}" --specs=nano.specs --specs=nosys.specs \
            -Wl,--gc-sections -Wl,--fatal-warnings "$work/$name/thing.o" "$work/$name/header_pool.o" \
            -L "$work/$name/prefix/lib" -lebisu_tio -lebisu_kii -lebisu_khc -lebisu_jkii \
            -lebisu_core -o "$work/$name/thing.elf"
        size "$work/$name/thing.elf" | awk 'NR > 1 { print $1, $2, $3 }'
    }
    {
        echo
        echo "### A thing on a Cortex-M3, $(arm-none-eabi-gcc --version | head -1)"
        echo
        echo "The program, newlib-nano included, linked with \`--gc-sections\` against the static libraries, in bytes."
        echo
        echo "| Build | Text | Data | Bss |"
        echo "|---|---|---|---|"
    } >> "$report"
    thing arm_minsizerel -- > "$work/sizes"
    read -r t d b < "$work/sizes"
    echo "| MinSizeRel | $t | $d | $b |" >> "$report"
    thing arm_min -- -DEBISU_MIN_SIZE=ON > "$work/sizes"
    read -r t d b < "$work/sizes"
    echo "| MinSizeRel, EBISU_MIN_SIZE | $t | $d | $b |" >> "$report"
    thing arm_lto -flto -- -DEBISU_MIN_SIZE=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON > "$work/sizes"
    read -r t d b < "$work/sizes"
    echo "| MinSizeRel, EBISU_MIN_SIZE, link-time optimization | $t | $d | $b |" >> "$report"
    thing arm_no_strtod -- -DEBISU_MIN_SIZE=ON -DEBISU_STRTOD=OFF > "$work/sizes"
    read -r t d b < "$work/sizes"
    echo "| MinSizeRel, EBISU_MIN_SIZE, without strtod() | $t | $d | $b |" >> "$report"
else
    printf '\nNo arm-none-eabi-gcc: no Cortex-M3 sizes.\n' >> "$report"
fi

cat "$report"
if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    cat "$report" >> "$GITHUB_STEP_SUMMARY"
fi
