# The large tests on FreeRTOS

The suites of [tests/large_test](../large_test), khc's, kii's and tio's, each
built as a firmware for the MPS2 AN385 board QEMU emulates, a Cortex-M3 with a
LAN9118 Ethernet controller, which runs the suite in a FreeRTOS task against
the live Kii application. CTest runs each in QEMU, whose user-mode networking
takes the board to the Internet. They are the host's suites, their sources as
[tests/large_test/CMakeLists.txt](../large_test/CMakeLists.txt) has them,
built with newlib and its C++ library, as [the small tests' firmware](../freertos_small)
is, and with what the large tests take from their platform,
[`ltest_platform.h`](../large_test/ltest_platform.h), the board's:
[`ltest_board.h`](ltest_board.h).

- **The socket:** platform's mbedTLS port over its FreeRTOS-Plus-TCP socket,
  with the [FreeRTOS sample's](../../tio/freertos-sample) network, DHCP and
  DNS, random generator, mbedTLS 3.6.7 and its configuration, and CA
  certificates, on FreeRTOS V11.3.3; or over its lwIP socket, as
  [below](#on-lwip). Plain TCP, for `EBISU_PLAIN_HTTP`, is the TCP socket
  alone.
- **The tasks the tests start**, as threads, are FreeRTOS tasks of 32 KB of
  stack each, at the suite's priority, joined through a semaphore; the
  tests' mutexes FreeRTOS mutexes; their condition variables check what they
  wait for at each notification, and every 10 ms.
- **The clocks:** the monotonic one is the tick count; the time of day the
  host's, in seconds, through semihosting, when first asked for, and the ticks
  since, for the names the tests make of it to differ.
- **The admin's credentials,** `CLIENT_ID` and `CLIENT_SECRET`, and `CI`,
  come from a file of the host's, `EBISU_LTEST_ENV_FILE`, lines of
  `NAME=value`, which the firmware reads through semihosting and sets in its
  environment before the suite starts. They never go on QEMU's command line,
  which CTest prints. A file named that hasn't them stops the firmware: the
  tests that need them would skip. The application's ID and site are built
  in, as on the host.
- **Left out:** `kii/openssl_socket_test.cpp`, which tests the OpenSSL port,
  on a POSIX host.

What a board does that a host doesn't shows in them: the mbedTLS port, without
`MBEDTLS_HAVE_TIME_DATE`, checks a certificate only at a clock's time, so each
socket the tests use has the tests' clock; and from QEMU, the chunk that ends
an empty body sent chunked comes after the headers, which the server answers
with `BAD_REQUEST` rather than `EMPTY_BODY`.

## On lwIP

The suites run over platform's lwIP socket too, on the kernels, lwIP and
mbedTLS that Realtek's SDKs ship: FreeRTOS V8.1.2 and lwIP 1.4.1, the SDK for
AmebaZ's, and V10.0.1 and lwIP 2.0.2, the SDK for AmebaZ2's, both with mbedTLS
2.4.0, configured as the SDKs configure them, in [lwip](lwip):

- **lwIP:** [lwip/port/lwipopts.h](lwip/port/lwipopts.h), the SDKs' settings
  wherever they bear on ebisu's socket: an OS without lwIP's core lock,
  timeouts of int milliseconds, sockets that set no errno and lwIP's own errno
  values, keepalive, IPv4 alone, and their pools, 5 KB of heap and 20 buffers
  of 500 bytes, and a window of 4 segments, or 2 with lwIP 2.0. Its port to
  FreeRTOS, [lwip/port/sys_arch.c](lwip/port/sys_arch.c), is the tests' own:
  FreeRTOS's semaphores, queues and tasks, made from the heap, as V8 has no
  other way.
- **The network:** lwIP's interface on the board's LAN9118,
  [lwip/lan9118_netif.c](lwip/lan9118_netif.c), over Arm's driver for it, from
  FreeRTOS-Plus-TCP's release, which ships it, with an address and a name
  server from DHCP.
- **TLS:** [lwip/mbedtls_config.h](lwip/mbedtls_config.h), the SDKs'
  `config_rsa.h`: TLS 1.2 at the most, RSA key exchange alone, AES in CBC
  mode, records of 4096 bytes at the most, no time, no threads, and no
  CTR_DRBG or entropy module. [lwip/random.c](lwip/random.c) builds CTR_DRBG
  for the tests alone, as the AmebaZ sample does, seeded from the host's
  `/dev/urandom`, through semihosting, under a FreeRTOS mutex, and sets
  mbedTLS's allocator, FreeRTOS's heap, as the samples do. As the samples,
  the tests ask the server for records no larger than mbedTLS takes, with the
  maximum fragment length extension, which Kii Cloud's API server ignores: a
  larger record would fail the connection.
- **The CA certificates:** the AmebaZ samples',
  [ca.pem](../../tio/amebaz-sample/ca.pem), whose RSA roots this mbedTLS,
  without ECDSA, reads.

What the boards have that QEMU's hasn't, the tests don't: Wi-Fi, in place of
Ethernet, and the chip's crypto engine, which the SDKs' mbedTLS runs AES in,
in software here.

## Running

A run takes about 5 minutes in QEMU, most of it the tests' own waits, as on a
host. As the host's, each suite is retried once, as they talk to a live
backend.

With CMake, the Arm GNU toolchain, newlib and its C++ library, and QEMU (on
Ubuntu, `cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
libstdc++-arm-none-eabi-newlib qemu-system-arm`), and `APP_ID`,
`DEFAULT_SITE`, `CLIENT_ID` and `CLIENT_SECRET` in the environment, as for the
host's large tests, from the repository's root:

```sh
tests/freertos_large/run.sh                  # FreeRTOS-Plus-TCP and mbedTLS 3.6.7 on V11.3.3
tests/freertos_large/run.sh V8.1.2 1.4.1     # lwIP 1.4.1 and mbedTLS 2.4.0 on V8.1.2
tests/freertos_large/run.sh V10.0.1 2.0.2    # lwIP 2.0.2 and mbedTLS 2.4.0 on V10.0.1
```

which builds in `build/freertos_large`, or `build/freertos_large-V8.1.2-lwip-1.4.1`
and the like, writes the credentials' file there,
readable by its user only, for the run, and CTest's JUnit report in
`test-results/junit`.
