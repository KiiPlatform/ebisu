# AmebaZ sample

tio on Realtek's AmebaZ, the RTL8710B and RTL8711B Wi-Fi SoCs, built with
Realtek's SDK for them,
[ameba-rtos-1](https://github.com/Ameba-AIoT/ameba-rtos-1), and with the
FreeRTOS V8.1.2, lwIP 1.4.1 and mbedTLS 2.4.0 the SDK ships, as it
configures them. The firmware joins a Wi-Fi network, onboards, uploads its
state periodically, and logs the commands it receives.

CI builds the firmware's images with the SDK, and fails on a warning in
ebisu's sources or the sample's. Nothing emulates the chip, so CI doesn't run
them, and no device runs them either: they are built, not tested.

- [main.c](main.c): in place of the SDK's example's: starts the SDK's console
  and Wi-Fi, then sets up tio as the [AmebaZ2 sample](../amebaz2-sample/main.c)
  does, with the settings `build.sh` is given.
- [net.c](net.c): joins the Wi-Fi network, and waits for an address from DHCP.
- [random.c](random.c): random numbers for TLS, from a CTR_DRBG of the
  sample's own: see [Randomness](#randomness).
- [header_pool.c](header_pool.c): a pool for the request headers, which with
  the buffers main.c gives tio leaves ebisu nothing to take from the heap.
- [log.c](log.c): the SDK's console, as the sample writes to it, and a logger
  for ebisu's ports, which say there why a connection failed.
- [build.sh](build.sh): builds it all into the SDK's firmware.

The rest is ebisu's ports:

- [`ebisu_freertos.h`](../../platform/freertos/include/ebisu_freertos.h): tasks,
  delays, the locks and the monotonic clock;
- [`ebisu_lwip.h`](../../platform/lwip/include/ebisu_lwip.h): TCP over lwIP's
  sockets, whose names lwIP's DNS client looks up;
- [`ebisu_mbedtls.h`](../../platform/mbedtls/include/ebisu_mbedtls.h): TLS with
  mbedTLS over it.

## Building

On Linux, x86-64, with the 32-bit C library, which the SDK's toolchain, GCC
4.8 of 2014, a 32-bit build, runs with (Ubuntu's `libc6:i386`), `make`, `gawk`,
`bzip2` and Python 3:

```sh
git clone https://github.com/Ameba-AIoT/ameba-rtos-1
EBISU_SAMPLE_VENDOR_THING_ID=... EBISU_SAMPLE_THING_PASSWORD=... \
EBISU_SAMPLE_WIFI_SSID=... EBISU_SAMPLE_WIFI_PASSWORD=... \
    tio/amebaz-sample/build.sh ameba-rtos-1
```

CI builds it with the SDK at commit `63d5484`, of 2022-11-23.

`build.sh` copies the SDK's example project, `project/realtek_amebaz_va0_example`,
to `project/ebisu_amebaz_sample`, in the SDK's checkout, and compiles the
sample's main.c in place of the example's, and ebisu's sources and the
sample's, copied into the project, with the SDK's sources, its compiler options
and toolchain, and with ebisu's and the sample's compiled with `-Wall` and
without the SDK's `-w`, which leaves out every warning. The images are
`boot_all.bin` and `image2_all_ota1.bin`, in
`project/ebisu_amebaz_sample/ebisu/out`. The SDK's makefile writes each object
next to its source, for every project of the checkout, and a project takes one
that is there as built, whatever options it was built with: `build.sh` builds
from nothing, and leaves the SDK's sources without objects, so that the next
project of the checkout, the SDK's example too, builds them from nothing too.

The settings are the environment's, as `EBISU_SAMPLE_*`:

| Variable | What it is | Default |
|---|---|---|
| `EBISU_SAMPLE_APP_ID` | The Kii Cloud application's ID | `fj9xy2fsp0ld` |
| `EBISU_SAMPLE_APP_HOST` | The application's API host | `api-jp.kii.com` |
| `EBISU_SAMPLE_VENDOR_THING_ID` | The thing's vendor thing ID | none |
| `EBISU_SAMPLE_THING_PASSWORD` | The thing's password | none |
| `EBISU_SAMPLE_THING_TYPE` | Its thing type, when it is onboarded the first time | none |
| `EBISU_SAMPLE_FIRMWARE_VERSION` | Its firmware version, likewise | none |
| `EBISU_SAMPLE_STATE` | The state it uploads, as JSON | an air conditioner's |
| `EBISU_SAMPLE_UPDATE_PERIOD` | How often, in seconds | 60 |
| `EBISU_SAMPLE_CLOCK_FLOOR` | The clock's floor, in seconds since 1970: see [The server's certificate](#the-servers-certificate) | 0 |
| `EBISU_SAMPLE_CA_FILE` | The CA certificates, PEM | [ca.pem](ca.pem) |
| `EBISU_SAMPLE_WIFI_SSID` | The Wi-Fi network | none |
| `EBISU_SAMPLE_WIFI_PASSWORD` | Its WPA2 passphrase, or none for an open network | none |

The password and the passphrase are in the image as they are given. Without a
network, the firmware waits for one joined from the SDK's console, with its
commands `ATW0=<ssid>`, `ATW1=<passphrase>` and `ATWC`.

## The flash

The SDK's example lays out a flash of 1 MB, with 468 KB for each of its two
images, OTA1's and OTA2's, which the firmware, with ebisu and mbedTLS, doesn't
fit in. The sample lays out one of 2 MB, in its copy of the example's linker
scripts and `platform_opts.h`:

| At | What |
|---|---|
| `0x000000` | The boot image, and the SDK's system and calibration data, as the example has them |
| `0x00B000` | OTA1, the application's image: up to 976 KB |
| `0x0FF000` | The Wi-Fi's fast reconnect data, at the top of OTA1's slot, as the example has it |
| `0x100000` | OTA2, for the next image |
| `0x1FC000` | The UART's and SPI's settings, and the AP's at `0x1FE000`, 1 MB on from the example's |

The SDK's OTA update takes OTA2 to be at `0x80000`, in OTA1 here, unless the
system data, at `0x9000`, says otherwise: `build.sh` builds the firmware
without it. An application that updates itself writes OTA2's address there
first, `0x08100000` as the chip maps the flash, with the SDK's
`ota_write_ota2_addr()`.

## What the SDK has, and the sample does with it

- **FreeRTOS V8.1.2**, configured with ticks of 32 bits at 1 kHz, mutexes and
  a heap, and without `configUSE_NEWLIB_REENTRANT`: newlib's `strtod()` works
  in the calling task's newlib state, and the handler's and the updater's tasks
  parse JSON at once, so `build.sh` builds jkii with its own number parser,
  `EBISU_NO_STRTOD`.
- **lwIP 1.4.1**, built without `LWIP_SO_SNDTIMEO`, which ebisu's lwIP socket
  needs for its send timeout, but for the SDK's UART adapter and AT commands:
  `build.sh` sets it to 1. lwIP is compiled from its sources with the firmware,
  and the SDK's libraries call only its sockets' functions. Its sockets set no
  `errno` without `ERRNO`, which the SDK defines for its AT commands' examples
  alone: ebisu's socket reads why a call failed from each socket's
  `SO_ERROR`.
- **TLS.** The SDK's example builds PolarSSL 1.3.8, whose cryptography is in
  the chip's ROM, and has mbedTLS 2.4.0 as sources, for its
  `CONFIG_USE_MBEDTLS`: `build.sh` sets that, in place of
  `CONFIG_USE_POLARSSL`, and compiles mbedTLS's library in place of
  PolarSSL's, which ebisu's port is for.
- **mbedTLS's allocator.** The SDK builds mbedTLS with
  `MBEDTLS_PLATFORM_MEMORY` and no allocator: main.c gives it FreeRTOS's heap.
- **The C library.** The SDK's `platform_stdlib.h` has the files that include
  it, mbedTLS's configuration among them, call the ROM's `printf`,
  `snprintf`, `vsnprintf` and string functions; and the SDK links newlib's
  `_write()` from libnosys, so that newlib's `printf` writes nowhere. The
  sample's log formats its lines with newlib's `vsnprintf()`, in a file that
  doesn't include it, and writes them with the ROM's printf, `rtl_printf()`.

## TLS

mbedTLS is the SDK's, configured as the SDK configures it, with its
`config_rsa.h`: TLS 1.2, which ebisu's port asks for at the least, with RSA
key exchange and AES in CBC mode, and no other cipher suites, ECDHE's or
GCM's among them; and records of 4096 bytes at the most, where a server may
send 16384. The sample asks the server for records no larger, with the maximum
fragment length extension, which a server that doesn't take it ignores: a
record larger than mbedTLS takes fails the connection. mbedTLS is built without
`MBEDTLS_THREADING_C`: the tasks' connections share the TLS configuration,
which none changes, and the sample's random generator, which has a lock of its
own.

### The server's certificate

The server's certificate has to chain to one of the CA certificates in
[ca.pem](ca.pem), Amazon Trust Services' roots, which Kii Cloud's chain to,
and be issued for the host. The SDK builds mbedTLS without
`MBEDTLS_HAVE_TIME_DATE`, and FreeRTOS keeps no time of day: ebisu's mbedTLS
port checks the certificates' validity periods at the time an
`ebisu_server_clock_t` keeps, which the device learns from the server. Until
the first response, the clock has only a floor, a time it is known not to be
earlier than, and only a certificate that expired before it can be told. The
[kii README](../../kii/README.md#the-time) describes the clock.

## Randomness

TLS is only as secret as the random numbers its keys are made from, and with
RSA key exchange the client makes the whole of them. The SDK builds mbedTLS
without a random generator of its own, and has no interface to a generator of
the chip's: the sample gives mbedTLS a CTR_DRBG, mbedTLS's own, compiled for
the sample alone, in random.c, with the SDK's configuration otherwise. It is
seeded from the noise of the chip's ADC, as the SDK's `Gen_RandomSeed()`
samples it, the least significant bit of 32 conversions for each 32-bit word:
each byte of the seed is from SHA-256 over 8 bytes of the noise, as how much
entropy each has is not known, and has not been measured on a device. The
sample seeds it once, before the Wi-Fi starts, as `Gen_RandomSeed()` sets the
ADC up and stops it, which the SDK does only as it starts; and doesn't start
on the one package the SDK doesn't sample the ADC on,
`SYSCFG_BD_QFN48_MCM_8MBFlash`. The tasks' connections take from the one
generator, under a lock of the sample's.

The SDK's TLS examples give mbedTLS `rtw_get_random_bytes()` instead. Don't:
give it a generator such as the sample's.

## Memory

Each of the sample's tasks has a stack of 12 KB: the application's, which
onboards and then ends, and the handler's and the updater's, which ebisu makes.
mbedTLS takes from FreeRTOS's heap, of 100 KB in the SDK's configuration with
its Wi-Fi, about 9 KB for each connection's records, and the tasks' stacks are
from it too. None of it has been measured on a device.

## On a device

Write `boot_all.bin` at `0x0` and `image2_all_ota1.bin` at `0xB000` in a flash
of 2 MB, over SWD, as the SDK's `make flash` does, and read the log on the
SDK's console, the chip's log UART.
