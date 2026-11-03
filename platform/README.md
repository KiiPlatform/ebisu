# platform {#platform_readme}

`platform` is what ebisu needs from the device it runs on, and the tools built
on that. It is the lowest layer: `jkii`, `khc`, `kii` and `tio` all sit on it, and it
depends on none of them.

It isn't a library of its own. `jkii`, `khc`, `kii` and `tio` each embed it, so
nothing more has to be linked. Without CMake, add `platform/src/*.c` to the sources and
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
  that need no NUL, and splitting on a delimiter without copying. No
  allocation, floating point, locale or `errno`.
- **Loggers** (`ebisu_log.h`): `ebisu_logger_t` is where a port's diagnostics
  go, each with a level, such as the application's own log or syslog.

## Ports

Implementations for a kind of system, each a static library of its own, built
where what it needs is found. Link one beside `libkhc`, `libkii` or `libtio`,
which have the rest of `platform`.

- **POSIX** (`ebisu_posix.h`, library `ebisu_posix`): `ebisu_posix_system()`,
  whose tasks are detached threads, whose delays are `nanosleep()` and whose
  monotonic clock is `CLOCK_MONOTONIC`, and `ebisu_posix_lock_t`, a pthread
  mutex. Needs POSIX threads.
- **OpenSSL** (`ebisu_openssl.h`, library `ebisu_openssl`):
  `ebisu_openssl_socket_t`, TLS 1.2 or later over a blocking BSD socket,
  taking in TLS 1.2 only suites with forward secrecy and authenticated
  encryption, Mozilla's intermediate list, and checking the
  server's certificate at an `ebisu_clock_t`'s time. It looks the host up with
  `getaddrinfo()` and tries each of its addresses, IPv6 and IPv4, both within
  its connect timeout. Its connections share one
  `SSL_CTX`: the system's CA certificates, or those of an
  `ebisu_openssl_tls_t` set up with a CA file or directory, for a device
  without the system's. The `SSL_CTX` keeps the TLS sessions of the last
  connections to a few hosts, and a connection offers the server the one for its
  host and port to resume, which saves most of the handshake when the server
  takes it. Its `last_error` says why an operation failed, which also goes to
  the logger set with `ebisu_openssl_socket_set_logger()`, or to stderr. Needs
  OpenSSL 1.0.2 or later, and POSIX threads.
- **Win32** (`ebisu_win32.h`, library `ebisu_win32`): `ebisu_win32_system()`,
  whose tasks are threads of their own, made with `_beginthreadex()`, whose
  delays are `Sleep()` and whose monotonic clock is `GetTickCount64()`, and
  `ebisu_win32_lock_t`, an SRWLOCK.
- **Schannel** (`ebisu_schannel.h`, library `ebisu_schannel`):
  `ebisu_schannel_socket_t`, TLS over Winsock 2 with Schannel, through SSPI,
  checking the server's certificate itself with crypt32, at an
  `ebisu_clock_t`'s time: a chain to a root in the Windows root store, valid
  for server authentication, issued for the host. It drives Schannel itself,
  keeping what it has received but not yet returned, and waits for a receive
  with `select()`, so that a timeout leaves the connection usable. Its
  `last_error` says why an operation failed, which also goes to the logger set
  with `ebisu_schannel_socket_set_logger()`, or to stderr. Needs only what
  Windows provides, `ws2_32`, `secur32` and `crypt32`, which it links.

The Linux sample uses the POSIX and OpenSSL ports, and the Windows sample the
Win32 and Schannel ones.

A daemon can send the OpenSSL port's diagnostics to syslog with a logger such as
this one. `vsyslog()` isn't POSIX, but glibc, musl and the BSDs have it:

```c
#include <ebisu_openssl.h>
#include <syslog.h>

static void to_syslog(ebisu_logger_t* self, ebisu_log_level_t level,
        const char* fmt, va_list args)
{
    static const int priorities[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };
    vsyslog(priorities[level], fmt, args);
}

static const ebisu_logger_ops_t syslog_ops = { to_syslog };
static ebisu_logger_t syslog_logger = { &syslog_ops };

    ebisu_openssl_socket_set_logger(&sock, &syslog_logger);
```

The samples for FreeRTOS and ESP32 have implementations of their own, under
`tio`.
