# platform {#platform_readme}

`platform` is what ebisu needs from the device it runs on, and the tools built
on that. It is the lowest layer: `khc`, `kii` and `tio` all sit on it, and it
depends on none of them.

It isn't a library of its own. `khc`, `kii` and `tio` each embed it, so nothing
more has to be linked. Without CMake, add `platform/src/*.c` to the sources and
`platform/include` to the include path.

## Objects, not loose callbacks

A socket, the system and a lock are each a struct holding a pointer to a
`const` table of operations. ebisu calls each operation with the object, `self`,
as its first argument. An implementation embeds the struct in one of its own,
which holds its state, and gets that back with `EBISU_CONTAINER_OF()`
(`ebisu_object.h`). The table is shared by every object of a kind, so on a
microcontroller it stays in flash and each object costs one pointer. ebisu keeps
a pointer to each object it is given, so the object has to outlive what it is
set on.

## What it provides

- **Sockets** (`ebisu_socket.h`): the callbacks that connect, send, receive and
  close. TLS happens inside them, so ebisu sees a TLS connection as a socket.
  [Writing callbacks](https://github.com/KiiPlatform/ebisu/blob/master/khc/README.md#writing-callbacks)
  says what they have to do.
- **The system and locks** (`ebisu_system.h`): `ebisu_system_t` creates
  tasks, delays them and reads a monotonic clock, and `ebisu_lock_t` is a lock.
- **The clock** (`ebisu_clock.h`): `ebisu_clock_t` keeps the time of day on a
  device without a real-time clock, for a TLS stack to check a certificate's
  validity period against. Socket callbacks use it directly, and `kii` and `tio`
  feed it with the time each response states.

## Ports

Implementations for a kind of system, each a static library of its own, built
where what it needs is found. Link one beside `libkhc`, `libkii` or `libtio`,
which have the rest of `platform`.

- **POSIX** (`ebisu_posix.h`, library `ebisu_posix`): `ebisu_posix_system()`,
  whose tasks are detached threads, whose delays are `nanosleep()` and whose
  monotonic clock is `CLOCK_MONOTONIC`, and `ebisu_posix_lock_t`, a pthread
  mutex. Needs POSIX threads.
- **OpenSSL** (`ebisu_openssl.h`, library `ebisu_openssl`):
  `ebisu_openssl_socket_t`, TLS over a blocking BSD socket, checking the
  server's certificate at an `ebisu_clock_t`'s time. Its connections share one
  `SSL_CTX`: the system's CA certificates, or those of an
  `ebisu_openssl_tls_t` set up with a CA file or directory, for a device
  without the system's. Needs OpenSSL 1.0.2 or later, and POSIX threads.

The Linux sample uses both. The samples for Windows, FreeRTOS and ESP32 have
implementations of their own, under `tio`.
