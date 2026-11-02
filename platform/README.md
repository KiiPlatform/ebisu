# platform {#platform_readme}

`platform` is what ebisu needs from the device it runs on, and the tools built
on that. It is the lowest layer: `khc`, `kii` and `tio` all sit on it, and it
depends on none of them.

It isn't a library of its own. `khc`, `kii` and `tio` each embed it, so nothing
more has to be linked. Without CMake, add `platform/src/*.c` to the sources and
`platform/include` to the include path.

## What it provides

- **Sockets** (`ebisu_socket.h`): the callbacks that connect, send, receive and
  close. TLS happens inside them, so ebisu sees a TLS connection as a socket.
  [Writing callbacks](https://github.com/KiiPlatform/ebisu/blob/master/khc/README.md#writing-callbacks)
  says what they have to do.
- **Tasks** (`ebisu_task.h`): the callbacks that create a task, delay it, take a
  lock and read a monotonic clock.
- **The clock** (`ebisu_clock.h`): `ebisu_clock_t` keeps the time of day on a
  device without a real-time clock, for a TLS stack to check a certificate's
  validity period against. Socket callbacks use it directly, and `kii` and `tio`
  feed it with the time each response states.

The samples under `tio` implement these for Linux, Windows, FreeRTOS and ESP32.
