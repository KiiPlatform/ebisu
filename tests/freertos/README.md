# The FreeRTOS port's tests

A firmware for the MPS2 AN385 board QEMU emulates, a Cortex-M3, that tests
[`ebisu_freertos.h`](../../platform/freertos/include/ebisu_freertos.h) on the
kernel: each check that fails prints a line on the console, and QEMU exits with
1 if any did. CTest runs it in QEMU. FreeRTOS-Kernel is built with its own CMake
and ebisu with its own, as an application builds them, and the board's support
is the [FreeRTOS sample's](../../tio/freertos-sample/board/board.h).

- **The system's tasks**: the entry runs with its parameter, in a task of its
  own, of the name, stack depth and priority given; the task's memory, and what
  started it, go back to the heap once it ends, and to the heap too when the
  task can't be made, for a stack the heap can't hold.
- **Delays**: at least the time asked for, as the tick counts tell it, n + 1
  ticks being n whole ones at least, and not much more; none for 0; and, with
  ticks of 16 bits, one longer than `portMAX_DELAY`, in turns.
- **The monotonic clock**, the kernel started 3 seconds before its tick count
  overflows, `configINITIAL_TICK_COUNT`: it never goes back, in this task or in
  two more reading it all the while, it counts the ticks, and it ends past what
  the tick count holds.
- **The lock**: two tasks adding to a counter under it, with a delay in
  between, lose nothing; a task waiting for it, without `INCLUDE_vTaskSuspend`,
  waits on past `portMAX_DELAY`'s timeout of 65535 ticks; and it takes from the
  heap only without `configSUPPORT_STATIC_ALLOCATION`, which it gives back.

`xTaskCatchUpTicks()` moves the tick count on for what would take minutes, in
turns of 1000 ticks.

Each test is built three ways, `EBISU_TEST_CONFIG`:

| | Ticks | Rate | Static allocation | Heap | `INCLUDE_vTaskSuspend` |
|---|---|---|---|---|---|
| `a` | 32 bits | 1000 Hz | yes | heap_4 | yes |
| `b` | 16 bits | 250 Hz | no | heap_4 | no |
| `c` | 32 bits | 100 Hz | yes | none | yes |

against FreeRTOS-Kernel V11.1.0, that of FreeRTOS LTS 202406, the oldest the
port supports, and V11.3.3, the latest, `EBISU_TEST_KERNEL`, as CI does.

With CMake, the Arm GNU toolchain, newlib and QEMU (on Ubuntu, `cmake
gcc-arm-none-eabi libnewlib-arm-none-eabi qemu-system-arm`), from the
repository's root:

```sh
tests/freertos/run.sh V11.1.0 a
```

which builds in `build/freertos-V11.1.0-a`, and writes CTest's JUnit report in
`test-results/junit`.
