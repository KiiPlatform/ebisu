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
  tasks, delays them, reads a monotonic clock, may make locks, and may give the
  time of day as a clock, and `ebisu_lock_t` is a lock. Inline calls such as
  `ebisu_system_delay_ms()`, `ebisu_system_lock_create()`,
  `ebisu_system_clock()` and `ebisu_lock_acquire()` call their operations, and
  the lock calls do nothing with no lock, for code that runs in one task.
- **The clock** (`ebisu_clock.h`, `ebisu_server_clock.h`): `ebisu_clock_t` gives
  the time of day, or a lower bound of it, for a TLS stack to check a
  certificate's validity period against; socket callbacks use it directly. The
  system's clock is one, and `ebisu_server_clock_t` is another, for a device
  without a real-time clock, which `kii` and `tio` feed with the time each
  response states, as a time source of the application's can.
- **Strings and numbers** (`ebisu_str.h`): bounded copies, a string builder
  over a fixed buffer that fails instead of cutting a string short, and that a
  function of the caller's, such as an escaper or an encoder, can write into,
  strict parsers of decimal, hexadecimal and fixed-point numbers from spans
  that need no NUL, bytes to hexadecimal and back, any bytes as one line of
  printable text for a log, and splitting on a delimiter without copying. No allocation, floating point, locale or `errno`.
- **Dates** (`ebisu_time.h`): a UTC date and time of the Gregorian calendar to
  seconds since the epoch and back, from year 1 to 9999, as `timegm()` and
  `gmtime()` would but without the C library, for a certificate's dates or an
  ISO 8601 timestamp. Seconds since the epoch written as one, as RFC 3339 has
  it, and whether a year is a leap year and how many days a month has, to check
  a date before converting it.
- **Name servers** (`ebisu_dns.h`): `ebisu_dns_t` lists the system's name
  servers and asks one of them a query over UDP, for platform's DNS probe, which
  asks them whether they answer before a lookup that can't be bounded.
  `ebisu_dns_parse_address()` reads a server's address, IPv4 in any form
  `inet_aton()` takes or IPv6 with a scope, and `ebisu_dns_format_address()`
  writes one, as RFC 5952 says, without the C library.
- **Loggers** (`ebisu_log.h`): `ebisu_logger_t` is where a port's diagnostics
  go, each with a level, such as the application's own log or syslog.
  `ebisu_log()` logs a message of the application's through one, and
  `ebisu_log_fanout_t` gives each message to several, such as `stderr` and a
  syslog server. `ebisu_log_level_filter_t` gives another the messages of a
  level that can change while it logs, and `ebisu_log_slot_t` holds a logger
  that can be changed or taken out while it logs, each under an `ebisu_lock_t`.
  `ebisu_log_level_parse()` reads a level's name, and `ebisu_log_level_name()`
  writes it. platform's `ebisu_loggers.h` has loggers to a stdio stream, to a
  file kept under a size, to syslog and to a syslog server over UDP.
- **Waits between attempts** (`ebisu_retry.h`): the wait after the n-th failure
  in a row, doubled from a first wait up to a longest one, of which a device
  waits half to all, its share from a seed such as an FNV-1a hash of its ID,
  so that devices that lost the cloud together don't come back together. In
  the caller's unit, with no clock, randomness or state; the MQTT task waits
  so between its attempts to reach the broker.
- **Clearing secrets** (`ebisu_mem.h`): `ebisu_secure_zero()` sets memory to 0
  in a way the compiler can't take for a store nothing reads, and leave out, as
  it may a `memset()` just before memory is freed or goes out of scope. ebisu
  clears with it what held a password, a token or MQTT credentials once done
  with them, and an application can with its own.

The sockets, the system, locks and name servers are implemented for POSIX with
OpenSSL, and for Windows with Schannel, by
[platform](https://github.com/KiiPlatform/ebisu/blob/master/platform/README.md)'s
ports.
