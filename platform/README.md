# platform {#platform_readme}

`platform` holds the ports: the abstractions of
[core](https://github.com/KiiPlatform/ebisu/blob/master/core/README.md), its
sockets, system and locks, implemented for a kind of system each, from what
that system provides. It also holds loggers, for where a port's diagnostics
and an application's own messages go, and a probe of the name servers, for a
lookup not to wait for ones that don't answer.

## Ports

Implementations for a kind of system, each a static library of its own, built
where what it needs is found, which links `libebisu_core`. Link one with
`libebisu_tio`, `libebisu_kii` or `libebisu_khc`.

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
  the logger set with `ebisu_openssl_socket_set_logger()`, if any, unless it is
  built with `-DEBISU_LOG=OFF`, and its `recv_wait` waits with `poll()`, after
  what OpenSSL has decrypted. Needs OpenSSL 1.0.2 or later, and POSIX threads.
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
  with `select()`, so that a timeout leaves the connection usable, as its
  `recv_wait` waits too. Its
  `last_error` says why an operation failed, which also goes to the logger set
  with `ebisu_schannel_socket_set_logger()`, if any, unless it is built with
  `-DEBISU_LOG=OFF`. Needs only what
  Windows provides, `ws2_32`, `secur32` and `crypt32`, which it links.

The Linux sample uses the POSIX and OpenSSL ports, and the Windows sample the
Win32 and Schannel ones.

The samples for FreeRTOS and ESP32 have implementations of their own, under
`tio`.

## Loggers

`ebisu_loggers.h`, library `ebisu_loggers`, built on POSIX and on Windows, has
loggers that take a port's diagnostics, or an application's own messages
through core's `ebisu_log()`. Each has a level, the least important it writes,
and drops what matters less, so a port can be given one at `EBISU_LOG_DEBUG`
and another at `EBISU_LOG_ERROR` together, through core's `ebisu_log_fanout_t`.
Each may be called from more than one task at once, and cuts a message longer
than `EBISU_LOGGER_LINE_MAX`, 1024 bytes, short with `...`.

Nothing in ebisu logs until it is given a logger, the ports included, and
`ebisu_log()` drops a message given none. To see a port's failures on stderr,
give it an `ebisu_stdio_logger_t` on `stderr` at `EBISU_LOG_WARNING`.

- **To a stdio stream** (`ebisu_stdio_logger_t`): a line of the level and the
  message, as in `error: failed to connect to example.com.`, to `stderr` or a
  file, written with one call and flushed.
- **To syslog** (`ebisu_syslog_logger_t`, on POSIX): `syslog()`, with a
  facility and the severity of the level. `openlog()` is the application's to
  call, if it wants to.
- **To a syslog server over UDP** (`ebisu_syslog_udp_logger_t`): a datagram for
  each message, formatted as RFC 5424 says, with the time to the millisecond,
  the host's name, the application's and the process's identifier, and sent as
  RFC 5426 does, to an IPv4 or IPv6 server looked up once, when the logger is
  initialized. Sending never waits: what the socket can't take at once is
  dropped, as is what the network loses.

```c
#include <ebisu_loggers.h>
#include <ebisu_openssl.h>
#include <syslog.h>

static ebisu_stdio_logger_t to_stderr;
static ebisu_syslog_logger_t to_syslog;
static ebisu_syslog_udp_logger_t to_server;
static ebisu_logger_t* loggers[] = { &to_stderr.base, &to_syslog.base, &to_server.base };
static ebisu_log_fanout_t logger;

    ebisu_stdio_logger_init(&to_stderr, stderr, EBISU_LOG_WARNING);
    ebisu_syslog_logger_init(&to_syslog, LOG_DAEMON, EBISU_LOG_INFO);
    /* A server that can't be looked up leaves a logger that drops everything. */
    ebisu_syslog_udp_logger_init(&to_server, "logs.example.com", 514, "my-device", 3,
            EBISU_LOG_DEBUG);
    ebisu_log_fanout_init(&logger, loggers, 3);

    ebisu_openssl_socket_set_logger(&sock, &logger.base);
    ebisu_log(&logger.base, EBISU_LOG_INFO, "started, version %s", "1.0");
    ...
    ebisu_syslog_udp_logger_close(&to_server);
```

The loggers' levels and the fan-out's array are set before they are used, and
don't change: a logger of the application's that wraps one can decide at run
time what it passes on.

## DNS probe

`ebisu_dns_probe.h`, library `ebisu_dns_probe`, built on POSIX, asks whether
the name servers answer before a host is looked up. `getaddrinfo()` can't be
given a timeout: when the servers don't answer, a lookup waits for as long as
the C library keeps asking them again, which can be minutes, and so does the
task that looked up.

`ebisu_dns_probe()` sends one query for the host to each server of
`/etc/resolv.conf` in turn, as the C libraries read it, or 127.0.0.1 without
one, until one answers, giving each the time it is given.
`ebisu_dns_probe_servers()` asks the servers it is given instead. Any answer
counts, that the name doesn't exist too: the probe tells only whether a server
answers. A host that needs no server, an address or localhost, needs no probe.

`ebisu_dns_probe_socket_t` probes before another socket connects, and fails the
connect if no server answers, with `EBISU_SOCK_ERROR_DNS`, rather than have
the other socket look the host up. The rest is the other socket's.

```c
#include <ebisu_dns_probe.h>
#include <ebisu_openssl.h>

static ebisu_openssl_socket_t tls;
static ebisu_dns_probe_socket_t sock;

    ebisu_openssl_socket_init(&tls, 30, 30);
    /* Each server has 3 seconds to answer. */
    ebisu_dns_probe_socket_init(&sock, &tls.base, 3000);
    ebisu_dns_probe_socket_set_logger(&sock, &logger.base);
    kii_set_http_socket(&kii, &sock.base);
```

`ebisu_syslog_udp_logger_init()` looks its server up too, once: probing the
server's name first tells whether that lookup would wait.

The probe is for a device whose lookups all go to the name servers. A name
that `/etc/hosts` or another source has still needs a server to answer the
probe.
