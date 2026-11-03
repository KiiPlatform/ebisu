# core {#core_readme}

`core` is what every component of ebisu is written against: the abstractions of
what it needs from the device it runs on, and the utilities they share. It is
the lowest layer: `jkii`, `khc`, `kii` and `tio` all sit on it, and it depends
on none of them.

It is a library of its own, `libebisu_core`, which the others link. Without
CMake, add `core/src/*.c` to the sources and `core/include` to the include
path.

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
  An optional `last_error` says why one failed, as an `ebisu_sock_error_t` and
  a detail, which khc, kii and tio pass on.
  [Writing callbacks](https://github.com/KiiPlatform/ebisu/blob/master/khc/README.md#writing-callbacks)
  says what they have to do.
- **The system and locks** (`ebisu_system.h`): `ebisu_system_t` creates
  tasks, delays them and reads a monotonic clock, and `ebisu_lock_t` is a lock.
- **The clock** (`ebisu_clock.h`): `ebisu_clock_t` keeps the time of day on a
  device without a real-time clock, for a TLS stack to check a certificate's
  validity period against. Socket callbacks use it directly, and `kii` and `tio`
  feed it with the time each response states.
- **Strings and numbers** (`ebisu_str.h`): bounded copies, a string builder
  over a fixed buffer that fails instead of cutting a string short, and that a
  function of the caller's, such as an escaper or an encoder, can write into,
  strict parsers of decimal, hexadecimal and fixed-point numbers from spans
  that need no NUL, bytes to hexadecimal and back, and splitting on a
  delimiter without copying. No allocation, floating point, locale or `errno`.
- **Dates** (`ebisu_time.h`): a UTC date and time of the Gregorian calendar to
  seconds since the epoch and back, from year 1 to 9999, as `timegm()` and
  `gmtime()` would but without the C library, for a certificate's dates or an
  ISO 8601 timestamp.
- **Loggers** (`ebisu_log.h`): `ebisu_logger_t` is where a port's diagnostics
  go, each with a level, such as the application's own log or syslog.
- **Clearing secrets** (`ebisu_mem.h`): `ebisu_secure_zero()` sets memory to 0
  in a way the compiler can't take for a store nothing reads, and leave out, as
  it may a `memset()` just before memory is freed or goes out of scope. ebisu
  clears with it what held a password, a token or MQTT credentials once done
  with them, and an application can with its own.

The sockets, the system and locks are implemented for POSIX with OpenSSL, and
for Windows with Schannel, by
[platform](https://github.com/KiiPlatform/ebisu/blob/master/platform/README.md)'s
ports.
