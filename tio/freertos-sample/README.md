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
- [sock_cb_freertos.c](sock_cb_freertos.c): the socket callbacks, mbedTLS over
  FreeRTOS-Plus-TCP's sockets.
- [sys_cb_freertos.c](sys_cb_freertos.c): tasks, delays and locks, and the
  monotonic clock on FreeRTOS's tick count.
- [net.c](net.c): FreeRTOS-Plus-TCP on the board's Ethernet, with an address
  from DHCP, and its hooks.
- [random.c](random.c): mbedTLS's random generator, which TLS and the TCP/IP
  stack share.
- [board](board/board.h): what is the board's: the start-up code, the linker
  script, the console, the source of randomness, and newlib's system calls.
- [config](config): FreeRTOS's, FreeRTOS-Plus-TCP's and mbedTLS's configuration.

FreeRTOS V11.3.2, FreeRTOS-Plus-TCP V4.4.2 and mbedTLS 3.6.7 are downloaded
when CMake configures the build, from their releases, and checked against
their hashes. ebisu's libraries are built from this repository's sources, and
its version header generated from its git history, as their own build does.

## The server's certificate

The server's certificate has to chain to one of the CA certificates in
[ca.pem](ca.pem), Amazon Trust Services' roots, which Kii Cloud's chain to, and
be issued for the host. mbedTLS checks all of that, but not the validity
periods: it would do so at the system's time, which the board does not have.
The socket callbacks check them in mbedTLS's verify callback instead, during
the handshake, at the time a `kii_clock_t` keeps, which the device learns from
the server. Until the first response, the clock has only a floor, a time it is
known not to be earlier than, and only a certificate that expired before it can
be told. The [kii README](../../kii/README.md#the-time) describes the clock.

## Randomness

TLS is only as secret as the random numbers its keys are made from, and
mbedTLS takes them from `board_entropy()`. QEMU's board has no hardware random
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
  to refuse the certificate;
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

The socket and system callbacks stay as they are.
