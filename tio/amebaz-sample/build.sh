#!/bin/bash
# Builds the sample into a firmware image with Realtek's AmebaZ SDK: the SDK's
# example project, copied next to it as ebisu_amebaz_sample, with the sample's
# main.c in place of the example's, the TLS library EBISU_SAMPLE_TLS names,
# mbedTLS in place of PolarSSL, or PolarSSL, the example's, and ebisu's sources
# and the sample's compiled into it, with the SDK's toolchain, as the SDK
# builds its own.
#
# Usage: tio/amebaz-sample/build.sh <sdk>
#   sdk: a checkout of https://github.com/Ameba-AIoT/ameba-rtos-1
#
# The sample's settings come from the environment, as EBISU_SAMPLE_*: see
# README.md. The images are boot_all.bin and image2_all_ota1.bin, in
# project/ebisu_amebaz_sample/ebisu/out in the SDK's checkout. It fails on a
# warning in ebisu's sources or the sample's, and on two sources of the same
# file name, which the SDK's makefile compiles to the same object.
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "usage: $0 <sdk>" >&2
    exit 2
fi
here=$(cd "$(dirname "$0")" && pwd)
ebisu=$(cd "$here/../.." && pwd)
sdk=$(cd "$1" && pwd)
example=$sdk/project/realtek_amebaz_va0_example
project=$sdk/project/ebisu_amebaz_sample
if [ ! -f "$example/GCC-RELEASE/application.mk" ]; then
    echo "$sdk has no AmebaZ example project" >&2
    exit 2
fi

: "${EBISU_SAMPLE_APP_ID:=fj9xy2fsp0ld}"
: "${EBISU_SAMPLE_APP_HOST:=api-jp.kii.com}"
: "${EBISU_SAMPLE_VENDOR_THING_ID:=}"
: "${EBISU_SAMPLE_THING_PASSWORD:=}"
: "${EBISU_SAMPLE_THING_TYPE:=}"
: "${EBISU_SAMPLE_FIRMWARE_VERSION:=}"
default_state='{"AirconAlias":{"PresetTemperature":20,"RoomTemperature":24}}'
: "${EBISU_SAMPLE_STATE:=$default_state}"
: "${EBISU_SAMPLE_UPDATE_PERIOD:=60}"
: "${EBISU_SAMPLE_CLOCK_FLOOR:=0}"
: "${EBISU_SAMPLE_CA_FILE:=$here/ca.pem}"
: "${EBISU_SAMPLE_WIFI_SSID:=}"
: "${EBISU_SAMPLE_WIFI_PASSWORD:=}"
# mbedtls, with ebisu's mbedTLS port, or polarssl, with its PolarSSL port,
# experimental.
: "${EBISU_SAMPLE_TLS:=mbedtls}"
case "$EBISU_SAMPLE_TLS" in
    mbedtls|polarssl) tls=$EBISU_SAMPLE_TLS ;;
    *) echo "EBISU_SAMPLE_TLS is mbedtls or polarssl, not $EBISU_SAMPLE_TLS" >&2; exit 2 ;;
esac
export EBISU_SAMPLE_APP_ID EBISU_SAMPLE_APP_HOST EBISU_SAMPLE_VENDOR_THING_ID \
    EBISU_SAMPLE_THING_PASSWORD EBISU_SAMPLE_THING_TYPE EBISU_SAMPLE_FIRMWARE_VERSION \
    EBISU_SAMPLE_STATE EBISU_SAMPLE_UPDATE_PERIOD EBISU_SAMPLE_CLOCK_FLOOR \
    EBISU_SAMPLE_WIFI_SSID EBISU_SAMPLE_WIFI_PASSWORD

# The example's sources and makefiles, without what a build of it left there.
mkdir -p "$project/inc" "$project/src" "$project/GCC-RELEASE" "$project/ebisu"
cp -R "$example/inc/." "$project/inc/"
cp -R "$example/src/." "$project/src/"
find "$example/GCC-RELEASE" -maxdepth 1 -type f -exec cp {} "$project/GCC-RELEASE/" \;
# The flash's layout, for 2 MB, where the SDK's example has 1 MB, whose 468 KB
# for each of its two images the firmware doesn't fit in: OTA1 at 0xB000, up to
# the Wi-Fi's fast reconnect data at 0xFF000, at the top of its slot as the
# example has it, OTA2 at 0x100000, and the settings 1 MB further on than the
# example's, past both. The linker scripts place the images.
for ld in "$project"/GCC-RELEASE/rlx8711B-symbol-v02-img2_xip1.ld \
        "$project"/GCC-RELEASE/rlx8711B-symbol-v02-img2_xip2.ld; do
    python3 -I - "$ld" <<'LAYOUT'
import re, sys

path = sys.argv[1]
text = open(path, newline='').read()
for region, origin in (('XIP1', '0x0800B000'), ('XIP2', '0x08100000')):
    pattern = re.compile(r'^(\s*%s \(rx\)\s*:\s*ORIGIN = )0x0[0-9A-Fa-f]+(\+0x20, LENGTH = )0x75000(-0x20)' % region, re.M)
    text, count = pattern.subn(lambda m: m.group(1) + origin + m.group(2) + '0xF4000' + m.group(3), text)
    if count != 1:
        sys.exit('%s: no single %s of 468 KB' % (path, region))
open(path, 'w', newline='').write(text)
LAYOUT
done
# The SDK's TLS, for its own users of it too, as its platform_opts.h chooses:
# mbedTLS in place of PolarSSL, or PolarSSL, the example's. Without its OTA
# update, which takes OTA2 to be at 0x80000, in OTA1 here, unless the system
# data says otherwise. And the settings the SDK keeps in the flash, past the
# images.
python3 -I - "$project/inc/platform_opts.h" "$tls" <<'OPTS'
import re, sys

path, tls = sys.argv[1:]
text = open(path, newline='').read()
mbedtls = tls == 'mbedtls'
# The first of each is the setting: the second, which sets PolarSSL if both or
# neither are set, or OTA for the AT commands' examples, doesn't apply then.
for name, value in (('CONFIG_USE_POLARSSL', '0' if mbedtls else '1'),
                    ('CONFIG_USE_MBEDTLS', '1' if mbedtls else '0'),
                    ('CONFIG_OTA_UPDATE', '0')):
    pattern = re.compile(r'^(#define[ \t]+%s[ \t]+)[01]' % name, re.M)
    text, count = pattern.subn(lambda m: m.group(1) + value, text, count=1)
    if count != 1:
        sys.exit('%s: no #define %s' % (path, name))
for name, was, now in (('AP_SETTING_SECTOR', '0x000FE000', '0x001FE000'),
                       ('UART_SETTING_SECTOR', '0x000FC000', '0x001FC000'),
                       ('SPI_SETTING_SECTOR', '0x000FC000', '0x001FC000'),
                       ('FAST_RECONNECT_DATA', '(0x80000 - 0x1000)', '(0x100000 - 0x1000)')):
    pattern = re.compile(r'^(#define[ \t]+%s[ \t]+)%s' % (name, re.escape(was)), re.M)
    text, count = pattern.subn(lambda m: m.group(1) + now, text)
    if count != 1:
        sys.exit('%s: no single #define %s %s' % (path, name, was))
open(path, 'w', newline='').write(text)
OPTS

# The sample's settings, its CA certificates and ebisu's version, generated.
gen=$project/ebisu
sh "$ebisu/version/ebisu_version.sh" "$gen/ebisu_version.h"
python3 -I - "$gen" "$EBISU_SAMPLE_CA_FILE" <<'PY'
import os, sys

gen, ca_file = sys.argv[1], sys.argv[2]
env = os.environ

def literal(text):
    out = '"'
    for byte in text.encode():
        char = chr(byte)
        if char in '"\\':
            out += '\\' + char
        elif 32 <= byte < 127:
            out += char
        else:
            out += '\\%03o' % byte
    return out + '"'

def number(name):
    value = env[name]
    if not value.isdigit():
        sys.exit('%s has to be a number, not %r' % (name, value))
    return value

strings = ['APP_ID', 'APP_HOST', 'VENDOR_THING_ID', 'THING_PASSWORD', 'THING_TYPE',
           'FIRMWARE_VERSION', 'STATE', 'WIFI_SSID', 'WIFI_PASSWORD']
lines = ['/* The sample\'s settings, generated by build.sh from the EBISU_SAMPLE_',
         '   variables of its environment. */',
         '#ifndef SAMPLE_CONFIG_H', '#define SAMPLE_CONFIG_H', '']
for name in strings:
    lines.append('#define EBISU_SAMPLE_%s %s' % (name, literal(env['EBISU_SAMPLE_' + name])))
lines.append('#define EBISU_SAMPLE_UPDATE_PERIOD %s' % number('EBISU_SAMPLE_UPDATE_PERIOD'))
lines.append('#define EBISU_SAMPLE_CLOCK_FLOOR %sLL' % number('EBISU_SAMPLE_CLOCK_FLOOR'))
lines += ['', '#endif /* SAMPLE_CONFIG_H */', '']
with open(os.path.join(gen, 'sample_config.h'), 'w') as f:
    f.write('\n'.join(lines))

with open(ca_file) as f:
    ca_lines = f.read().splitlines()
lines = ['/* The CA certificates of EBISU_SAMPLE_CA_FILE, generated by build.sh. */',
         '#include <stddef.h>', '', 'const char ca_pem[] =']
lines += ['    %s' % literal(line + '\n') for line in ca_lines]
lines += ['    "";', '',
          '/* With the terminating NUL, which mbedTLS takes PEM with. */',
          'const size_t ca_pem_size = sizeof(ca_pem);', '']
with open(os.path.join(gen, 'ca_pem.c'), 'w') as f:
    f.write('\n'.join(lines))
PY

# What the firmware compiles of ebisu: its libraries, and the FreeRTOS, lwIP
# and mbedTLS or PolarSSL ports, as the SDK has those; and the sample, with the
# TLS library's files of it, of their names, and not the other's. Copied into the
# project, as the SDK's makefile writes each object next to its source, and
# afresh, without what an earlier build copied.
src=$gen/src
rm -rf -- "$src"
mkdir -p "$src"
for dir in core jkii khc kii/include tio/include platform/freertos platform/lwip \
        "platform/$tls"; do
    mkdir -p "$src/$dir"
    cp -R "$ebisu/$dir/." "$src/$dir/"
done
cp "$ebisu"/kii/*.c "$ebisu"/kii/*.h "$src/kii/"
cp "$ebisu"/tio/*.c "$ebisu"/tio/*.h "$src/tio/"
mkdir -p "$src/sample"
cp "$here"/*.h "$src/sample/"
for file in "$here"/*.c; do
    case "$(basename "$file")" in
        *_mbedtls.c|*_polarssl.c) ;;
        *) cp "$file" "$src/sample/" ;;
    esac
done
cp "$here"/*_"$tls".c "$src/sample/"
sources=(
    "$src"/core/src/*.c
    "$src"/jkii/src/*.c
    "$src"/khc/src/*.c
    "$src"/kii/*.c
    "$src"/tio/*.c
    "$src/platform/freertos/src/ebisu_freertos.c"
    "$src/platform/lwip/src/ebisu_lwip_tcp.c"
    "$src/platform/$tls/src/ebisu_$tls.c"
    "$src"/sample/*.c
    "$gen/ca_pem.c"
)
includes=(
    "$src/core/include" "$src/core/src"
    "$src/jkii/include" "$src/jkii/src" "$src/jkii/libs"
    "$src/khc/include" "$src/khc/src"
    "$src/kii/include" "$src/kii"
    "$src/tio/include" "$src/tio"
    "$src/platform/freertos/include" "$src/platform/lwip/include"
    "$src/platform/$tls/include"
    "$src/sample" "$gen"
)
# mbedTLS, all of its library, which the SDK's makefile lists none of, as it
# builds PolarSSL; and its library's directory, for random_mbedtls.c, which
# compiles its CTR_DRBG. PolarSSL as the SDK's makefile has it, but for
# the sample's files of it.
mbedtls_sources=()
if [ "$tls" = mbedtls ]; then
    mbedtls=$sdk/component/common/network/ssl/mbedtls-2.4.0
    if [ ! -f "$mbedtls/library/ctr_drbg.c" ]; then
        echo "$sdk has no mbedTLS 2.4.0" >&2
        exit 2
    fi
    mbedtls_sources=("$mbedtls"/library/*.c)
    includes+=("$mbedtls/library")
fi
# The lwIP port's send timeout, which the SDK's lwIP is built without but for
# its UART adapter and AT commands: lwIP is compiled from its sources, and the
# SDK's libraries call only its sockets' functions. jkii's own number parser,
# which CI tests, in place of newlib's strtod(), which works in the calling
# task's newlib state, where the SDK's FreeRTOS is configured without
# configUSE_NEWLIB_REENTRANT.
defines=(-DLWIP_SO_SNDTIMEO=1 -DEBISU_NO_STRTOD)
if [ "$tls" = polarssl ]; then
    defines+=(-DEBISU_SAMPLE_POLARSSL)
fi

python3 -I - "$project/GCC-RELEASE/application.mk" "$tls" "${#sources[@]}" "${sources[@]}" \
        "${#mbedtls_sources[@]}" "${mbedtls_sources[@]}" \
        "${#includes[@]}" "${includes[@]}" "${defines[@]}" <<'PY'
import os, re, sys

mk, tls, args = sys.argv[1], sys.argv[2], sys.argv[3:]
n = int(args[0]); sources = args[1:1 + n]; args = args[1 + n:]
n = int(args[0]); mbedtls_sources = args[1:1 + n]; args = args[1 + n:]
n = int(args[0]); includes = args[1:1 + n]; defines = args[1 + n:]

text = open(mk).read()
main = 'SRC_C += ../src/main.c\n'
if text.count(main) != 1:
    sys.exit('%s: no single %r to put the sample in place of' % (mk, main.strip()))
# PolarSSL's library, which mbedTLS's takes the place of, file names and all.
polarssl = re.compile(r'^SRC_C \+= \S*/polarssl-1\.3\.8/library/\S+\.c\n', re.M)
if not polarssl.findall(text):
    sys.exit('%s: no PolarSSL library' % mk)
if tls == 'mbedtls':
    text = polarssl.sub('', text)
# Every source's object is named by its file name alone, in one directory.
listed = re.findall(r'^\s*(?:SRC_C|DRAM_C)\s*\+?=\s*(\S+)', text, re.M)
names = {}
for path in [p for p in listed if p != '../src/main.c'] + sources + mbedtls_sources:
    names.setdefault(os.path.basename(path), []).append(path)
clashes = {name: paths for name, paths in names.items() if len(paths) > 1}
if clashes:
    for name, paths in sorted(clashes.items()):
        print('sources named %s: %s' % (name, ' '.join(paths)), file=sys.stderr)
    sys.exit(1)

block = '# ebisu, its sample and its TLS, from tio/amebaz-sample/build.sh.\n'
block += ''.join('SRC_C += %s\n' % path for path in sources + mbedtls_sources)
block += ''.join('INCLUDES += -I%s\n' % path for path in includes)
text = text.replace(main, block)
# After the makefile sets CFLAGS: its recipes take it as it is at the end.
text += '\n# From tio/amebaz-sample/build.sh.\nCFLAGS += %s\n' % ' '.join(defines)
# The SDK builds with -w, which leaves out every warning, and without -Wall:
# ebisu's sources and the sample's, with -Wall, and without -w, as they are
# what build.sh checks. The SDK's, mbedTLS's or PolarSSL's among them, as the
# SDK has them.
text += 'EBISU_SAMPLE_O = %s\n' % ' '.join(os.path.splitext(s)[0] + '.o' for s in sources)
text += '$(EBISU_SAMPLE_O): CFLAGS := $(filter-out -w,$(CFLAGS)) -Wall\n'
# OTA2 where the layout has it, for the OTA tool, which a build of OTA2's
# image runs.
ota = ' 0x08080000 0x20170111 '
if text.count(ota) != 1:
    sys.exit('%s: no single OTA2 address for the OTA tool' % mk)
text = text.replace(ota, ' 0x08100000 0x20170111 ')
open(mk, 'w').write(text)
PY

cd "$project/GCC-RELEASE"
log=$gen/make.log
# The SDK's makefile writes each source's object next to it, for its every
# project, and a project takes one that is there as built: from nothing, as
# another project, such as the SDK's example, leaves them built with other
# options, lwIP's too; and, however this ends, without leaving them for the
# next project, which would take them, or fail to link without them.
trap 'make clean > /dev/null 2>&1' EXIT
make clean > /dev/null 2>&1
set +e
make all 2>&1 | sed 's/\x1b\[[0-9;]*[mK]//g' > "$log"
status=${PIPESTATUS[0]}
set -e
# What the compiler said of ebisu's sources and the sample's, and of their
# headers, wherever they were included from.
ours=$(grep -E "^$gen/[^:]+:[0-9]+:([0-9]+:)? (warning|error)" "$log" | sort -u || true)
if [ "$status" -ne 0 ]; then
    tail -40 "$log" >&2
    echo "make all failed: $status; the log is $log" >&2
    exit 1
fi
if [ -n "$ours" ]; then
    echo "$ours" >&2
    echo "warnings in ebisu's sources or the sample's; the log is $log" >&2
    exit 1
fi

# Out of what make clean removes: the boot image, at 0 in the flash, and the
# application's, at 0xB000, as the SDK's image tool writes them.
bin=$project/GCC-RELEASE/application/Debug/bin
out=$gen/out
mkdir -p "$out"
for file in boot_all.bin image2_all_ota1.bin application.axf application.map; do
    if [ ! -s "$bin/$file" ]; then
        echo "no $bin/$file" >&2
        exit 1
    fi
    cp "$bin/$file" "$out/"
done
size=$(find "$sdk/tools" -name arm-none-eabi-size -type f | head -n 1)
if [ -n "$size" ]; then
    "$size" "$out/application.axf"
fi
echo "the images: $out/boot_all.bin and $out/image2_all_ota1.bin"
