# FreeRTOS sample

tio on [FreeRTOS](https://www.freertos.org/), with
[FreeRTOS-Plus-TCP](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/)
and [mbedTLS](https://www.trustedfirmware.org/projects/mbed-tls/), for the MPS2
AN385 board QEMU emulates: a Cortex-M3 with a LAN9118 Ethernet controller. The
thing onboards, uploads its state periodically, and logs the commands it
receives. It runs on a desktop, in QEMU, whose user-mode networking reaches the
internet without privileges. CI runs it there against Kii Cloud.

- [main.c](main.c): starts the network, then sets up tio as the [Linux
  sample](../linux-sample/example.c) does, with the settings CMake is given.
  The buffers it gives tio, the request headers' among them, leave ebisu
  nothing to take from the heap.
- [net.c](net.c): FreeRTOS-Plus-TCP on the board's Ethernet, with an address
  from DHCP, and its hooks.
- [random.c](random.c): PSA's random generator, which TLS and the TCP/IP stack
  share, seeded from the board's source of randomness.
- [mbedtls_threading.c](mbedtls_threading.c): FreeRTOS mutexes for mbedTLS,
  with `MBEDTLS_THREADING_ALT`, as the tasks' connections share PSA's random
  generator.
- [log.c](log.c): the console, as the sample writes to it, and a logger for
  ebisu's ports, which say there why a connection failed.
- [board](board/board.h): what is the board's: the start-up code, the linker
  script, the console, the source of randomness, and newlib's system calls.
- [config](config): FreeRTOS's, FreeRTOS-Plus-TCP's and mbedTLS's configuration.

The rest is ebisu's ports:

- [`ebisu_freertos.h`](../../platform/freertos/include/ebisu_freertos.h): tasks,
  delays, the locks and the monotonic clock;
- [`ebisu_freertos_tcp.h`](../../platform/freertos/include/ebisu_freertos_tcp.h):
  TCP over FreeRTOS-Plus-TCP;
- [`ebisu_mbedtls.h`](../../platform/mbedtls/include/ebisu_mbedtls.h): TLS with
  mbedTLS over it. Each connection's `last_error` says why it failed, which the
  sample logs when onboarding fails.

FreeRTOS V11.3.3, FreeRTOS-Plus-TCP V4.4.2 and mbedTLS 3.6.7 are downloaded
when CMake configures the build, from their releases, and checked against
their hashes. FreeRTOS is built with its own CMake, and FreeRTOS-Plus-TCP and
mbedTLS from their sources, mbedTLS's library under the name its own CMake
gives it, `MbedTLS::mbedtls`. ebisu is built with its own CMake, added after
them, which builds its ports as their targets are there, and generates its
version header from the repository's git history.

## The server's certificate

The server's certificate has to chain to one of the CA certificates in
[ca.pem](ca.pem), Amazon Trust Services' roots, which Kii Cloud's chain to, and
be issued for the host. mbedTLS checks all of that, but not the validity
periods: it would do so at the system's time, which the board does not have.
ebisu's mbedTLS port checks them in mbedTLS's verify callback instead, during
the handshake, at the time an `ebisu_server_clock_t` keeps, which the device
learns from the server, and which the sample sets on its system as the system's
clock. Until the first response, the clock has only a floor, a time it is known
not to be earlier than, and only a certificate that expired before it can be
told. The [kii README](../../kii/README.md#the-time) describes the clock.

## Randomness

TLS is only as secret as the random numbers its keys are made from, which
mbedTLS takes from PSA's generator, seeded from `board_entropy()`. QEMU's board has no hardware random
number generator, so in QEMU it reads the host's `/dev/urandom`, through
semihosting. A device has to use its own hardware random number generator, or
another source of true randomness, instead.

## Building

With CMake, the Arm GNU toolchain and newlib (on Ubuntu, `cmake
gcc-arm-none-eabi libnewlib-arm-none-eabi`), from this directory:

```sh
cmake -B build -D EBISU_SAMPLE_VENDOR_THING_ID=... -D EBISU_SAMPLE_THING_PASSWORD=...
cmake --build build
```

The settings, in [CMakeLists.txt](CMakeLists.txt), are those of the ESP32
sample's menuconfig: the application's ID and host, the vendor thing ID and
password the thing onboards with, the thing type, the state it uploads, which
has to match how the application defines the thing type, and how often. And:

- `EBISU_SAMPLE_CLOCK_FLOOR`: a time, in seconds since the epoch, the time is
  known not to be earlier than, if later than when ebisu's source was committed:
  a device would save the time now and then, and set it as the floor when it
  starts again.
- `EBISU_SAMPLE_CA_FILE`: the CA certificates, if not `ca.pem`.

## Running in QEMU

QEMU's Arm emulator, `qemu-system-arm` (on Ubuntu, `qemu-system-arm`):

```sh
qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -display none -monitor none \
    -serial stdio -semihosting-config enable=on,target=native \
    -netdev user,id=net0 -net nic,model=lan9118,netdev=net0 \
    -kernel build/ebisu_freertos_sample.elf
```

[tests/samples/qemu_sample.py](../../tests/samples/qemu_sample.py) is how CI runs
it:

- with the clock's floor after Kii Cloud's certificate expires, the sample has
  to refuse the certificate, and its socket's `last_error` has to say so;
- against the application CI tests with, the sample onboards a thing of the
  run's own, its state reaches the thing's owner, and so does the result of a
  command the owner sends.

## On a device

The board's directory is what changes, and the configuration with it:

- the start-up code and the linker script, for the device's memory;
- `board_write()`, for its console;
- `board_entropy()`, for its random number generator;
- the network: FreeRTOS-Plus-TCP's driver for the device's network controller,
  in place of the LAN9118's, and its MAC address in [net.c](net.c);
- the port of FreeRTOS for its processor, and `configCPU_CLOCK_HZ`.

ebisu's ports stay as they are.
