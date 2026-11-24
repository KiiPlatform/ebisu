# The small tests on FreeRTOS

The suites of [tests/small_test](../small_test), each built as a firmware for
the MPS2 AN385 board QEMU emulates, a Cortex-M3, with the Arm GNU toolchain's
newlib and its C++ library, which runs the suite in a FreeRTOS task. CTest runs
each in QEMU. They are the host's suites, their sources and libraries as
[tests/small_test/CMakeLists.txt](../small_test/CMakeLists.txt) has them, so a
file added there is built here too. FreeRTOS-Kernel is built with its own CMake
and ebisu with its own, as an application builds them, and the board's support
is the [FreeRTOS sample's](../../tio/freertos-sample/board/board.h).

- **The suites:** core, khc, kii, jkii, jkii without `strtod()`, and tio, those
  tests/small_test has for every platform. Its others test the POSIX, Win32
  and mbedTLS ports and the loggers, which aren't built for the board.
- **Left out:** `core/log_test.cpp` and `kii/session_test.cpp`, which run
  `std::thread`s, which newlib's C++ library hasn't. [CMakeLists.txt](CMakeLists.txt)
  lists them, and why.
- **Catch's `main()`** is renamed, and called in the suite's task with the
  arguments QEMU has for it, through semihosting, the first of them the
  suite's name, then `--colour-mode none`: Catch can't tell semihosting's
  output isn't a terminal. A suite without test cases fails, as one would
  whose constructors never ran, with a message that says so.
- **Files and output** go through semihosting, with newlib's librdimon: the
  suites open their data files in their working directory, which QEMU is
  started in, as on the host, and Catch writes its console report.
- **Locales:** newlib has only C's, so the tests that look for a locale with a
  decimal comma, or one whose `toupper('i')` isn't `'I'`, are skipped, and the
  one that looks for a German one warns that it isn't changed.
- **The report** is CTest's JUnit one, an entry for each suite: Catch's JUnit
  reporter keeps every assertion's result until the end, more than the board's
  4 MB of RAM hold for jkii's 195,000.
- **Failing:** `small_test_tio_fails` runs tio's suite for a test case it
  hasn't, which Catch fails, for QEMU to exit with 1, as a suite that fails
  must.

Each suite prints how much of its task's stack, 256 KB, it never used. They
take about 90 seconds in QEMU, 70 of them core's checks of every day to the
year 9999, in 64-bit arithmetic on a 32-bit processor. FreeRTOS-Kernel is
V11.3.3, the latest: the suites ask nothing of it but a task.

With CMake, the Arm GNU toolchain, newlib and its C++ library, and QEMU (on
Ubuntu, `cmake gcc-arm-none-eabi libnewlib-arm-none-eabi
libstdc++-arm-none-eabi-newlib qemu-system-arm`), from the repository's root:

```sh
tests/freertos_small/run.sh
```

which builds in `build/freertos_small`, and writes CTest's JUnit report in
`test-results/junit`. To give a suite Catch's arguments, such as a test case's
name or `-d yes` for the time each takes, run it in QEMU from its working
directory, each argument an `arg=`, without spaces, as semihosting joins them
with spaces:

```sh
cd tests/small_test/khc
qemu-system-arm -machine mps2-an385 -cpu cortex-m3 -display none -monitor none \
    -serial stdio -semihosting-config enable=on,target=native,arg=small_test_khc,arg=--colour-mode,arg=none,arg=-d,arg=yes \
    -kernel ../../../build/freertos_small/small_test/small_test_khc.elf
```
