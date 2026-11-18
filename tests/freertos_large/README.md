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
  DNS, random generator, mbedTLS configuration and CA certificates. Plain TCP,
  for `EBISU_PLAIN_HTTP`, is the FreeRTOS-Plus-TCP socket alone.
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

A run takes about 5 minutes in QEMU, most of it the tests' own waits, as on a
host. As the host's, each suite is retried once, as they talk to a live
backend.

With CMake, the Arm GNU toolchain, newlib and its C++ library, and QEMU (on
Ubuntu, `cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
libstdc++-arm-none-eabi-newlib qemu-system-arm`), and `APP_ID`,
`DEFAULT_SITE`, `CLIENT_ID` and `CLIENT_SECRET` in the environment, as for the
host's large tests, from the repository's root:

```sh
tests/freertos_large/run.sh
```

which builds in `build/freertos_large`, writes the credentials' file there,
readable by its user only, for the run, and CTest's JUnit report in
`test-results/junit`.
