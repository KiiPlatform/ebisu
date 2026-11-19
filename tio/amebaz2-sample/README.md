# AmebaZ2 sample

tio on Realtek's AmebaZ2, the RTL8710C Wi-Fi SoC, built with its SDK,
[ameba-rtos-z2](https://github.com/Ameba-AIoT/ameba-rtos-z2), and with the
FreeRTOS V10.0.1, lwIP 2.0.2 and mbedTLS 2.4.0 the SDK ships, as it
configures them. The firmware joins a Wi-Fi network, onboards, uploads its
state periodically, and logs the commands it receives.

CI builds the firmware image with the SDK, and fails on a warning in ebisu's
sources or the sample's. Nothing emulates the chip, so CI doesn't run the
image, and no device runs it either: it is built, not tested.

- [main.c](main.c): in place of the SDK's example's: starts the SDK's console
  and Wi-Fi, then sets up tio as the [FreeRTOS
  sample](../freertos-sample/main.c) does, with the settings `build.sh` is
  given.
- [net.c](net.c): joins the Wi-Fi network, and waits for an address from DHCP.
- [random.c](random.c): random numbers for TLS, from the chip's crypto engine:
  see [Randomness](#randomness).
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

On Linux, x86-64, where the SDK's toolchain runs, with `make`, `bc`, `bzip2`,
`file` and Python 3 as `python`, which the SDK's scripts run (Ubuntu's
`python-is-python3`):

```sh
git clone https://github.com/Ameba-AIoT/ameba-rtos-z2
EBISU_SAMPLE_VENDOR_THING_ID=... EBISU_SAMPLE_THING_PASSWORD=... \
EBISU_SAMPLE_WIFI_SSID=... EBISU_SAMPLE_WIFI_PASSWORD=... \
    tio/amebaz2-sample/build.sh ameba-rtos-z2
```

CI builds it with the SDK at commit `1e5a4ad`, of 2026-10-23.

`build.sh` copies the SDK's example project, `project/realtek_amebaz2_v0_example`,
to `project/ebisu_amebaz2_sample`, in the SDK's checkout, and compiles the
sample's main.c in place of the example's, and ebisu's sources and the
sample's, copied into the project, with the SDK's sources, its compiler options
and toolchain. The image is `project/ebisu_amebaz2_sample/ebisu/out/flash_is.bin`.
The SDK's makefile writes each object next to its source, for every project of
the checkout, and a project takes one that is there as built, whatever options
it was built with: `build.sh` builds from nothing, and leaves the SDK's sources
without objects, so that the next project of the checkout, the SDK's example
too, builds them from nothing too.

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

## What the SDK has, and the sample does with it

- **lwIP's send timeout.** The SDK builds lwIP without `LWIP_SO_SNDTIMEO`,
  which ebisu's lwIP socket needs for its send timeout: `build.sh` sets it to
  1. lwIP is compiled from its sources with the firmware, and the SDK's
  libraries call only its sockets' functions, so no library has lwIP's
  structures as they were.
- **lwIP's errors.** The SDK's lwIP 2.0 is built with `LWIP_SOCKET_SET_ERRNO`
  at 0: its sockets set no `errno`, and ebisu's socket reads why a call
  failed from each socket's `SO_ERROR`.
- **`strtod()`.** The SDK links `strtod()`, as it does the C library's other
  string functions, to the ROM's, with `-Wl,-wrap`. `build.sh` builds jkii
  with its own number parser, `EBISU_NO_STRTOD`, which CI tests, in its place.
- **mbedTLS's allocator.** The SDK builds mbedTLS with
  `MBEDTLS_PLATFORM_MEMORY` and no allocator: main.c gives it FreeRTOS's heap.
- **The console.** The SDK's `printf` family is its own, which writes nothing
  for some formats, `%e`, `%g` or `%o` among them; the sample's messages and
  ebisu's use none of those.

## TLS

mbedTLS is the SDK's, configured as the SDK configures it, with its
`config_rsa.h`: TLS 1.2, which ebisu's port asks for at the least, with RSA
key exchange and AES in CBC mode, and no other cipher suites, ECDHE's or
GCM's among them; and records of 4096 bytes at
the most, where a server may send 16384. The sample asks the server for
records no larger, with the maximum fragment length extension, which a server
that doesn't take it ignores: a record larger than mbedTLS takes fails the
connection. mbedTLS is built without `MBEDTLS_THREADING_C`: the tasks'
connections share the TLS configuration, which none changes, and the sample's
random generator, which has a lock of its own.

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
without a random generator of its own, and the sample gives it
`crypto_random_generate()`'s: the crypto engine derives what it gives, with
HKDF, from 80 bytes of timer jitter it samples afresh for each call, a bit for
each tick of a 31-microsecond timer: some 20 ms. So the sample takes 256 bytes
at a time, and gives each once, under a lock of its own, and with the SDK's
lock of the crypto engine, which mbedTLS's hardware AES uses too.

The SDK's TLS examples give mbedTLS `rtw_get_random_bytes()` instead. Don't:
take the crypto engine's generator, as the sample does.

## Memory

Each of the sample's tasks has a stack of 12 KB: the application's, which
onboards and then ends, and the handler's and the updater's, which ebisu makes.
mbedTLS takes from FreeRTOS's heap, of 160 KB in the SDK's configuration,
about 9 KB for each connection's records. None of it has been measured on a
device.

## On a device

Flash `flash_is.bin` with the SDK's image tool, in `tools/AmebaZ2/Image_Tool`,
and read the log on the SDK's console, the chip's log UART.
