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
  monotonic clock is `CLOCK_MONOTONIC`, `ebisu_posix_lock_t`, a pthread
  mutex, and `ebisu_posix_tcp_socket_t`, plain TCP over a blocking BSD socket,
  for a server without TLS, as an MQTT broker's port is with
  `kii_enable_insecure_mqtt()`. Nothing it sends is encrypted. It looks the
  host up and connects as the OpenSSL port's socket does, with the same code,
  and a lookup thread of its own for a host and port. A send to a peer that
  has gone fails, with `MSG_NOSIGNAL`, or `SO_NOSIGPIPE` where send() has no
  such flag, rather than raise `SIGPIPE`. Its `last_error` says why an
  operation failed, which also goes to the logger set with
  `ebisu_posix_tcp_socket_set_logger()`, if any, unless it is built with
  `-DEBISU_LOG=OFF`, and its `recv_wait` waits with `poll()`. Needs POSIX
  threads.
- **OpenSSL** (`ebisu_openssl.h`, library `ebisu_openssl`):
  `ebisu_openssl_socket_t`, TLS 1.2 or later over a blocking BSD socket,
  taking in TLS 1.2 only suites with forward secrecy and authenticated
  encryption, Mozilla's intermediate list, and checking the
  server's certificate at an `ebisu_clock_t`'s time. It looks the host up with
  `getaddrinfo()` and tries each of its addresses, IPv6 and IPv4, both within
  its connect timeout. A lookup the timeout cuts short goes on on a thread of
  its own, which later connects to the same host and port wait for rather than
  look it up again. Its connections share one
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
  delays are `Sleep()` and whose monotonic clock is `GetTickCount64()`,
  `ebisu_win32_lock_t`, an SRWLOCK, and `ebisu_win32_tcp_socket_t`, plain TCP
  over Winsock 2, for a server without TLS. Nothing it sends is encrypted. It
  looks the host up and connects as the Schannel port's socket does, with the
  same code, and waits for a receive with `select()`, as it does too. Its
  `last_error` and logger are as the Schannel port's. Needs only what Windows
  provides, `ws2_32`, which it links.
- **Schannel** (`ebisu_schannel.h`, library `ebisu_schannel`):
  `ebisu_schannel_socket_t`, TLS over Winsock 2 with Schannel, through SSPI,
  checking the server's certificate itself with crypt32, at an
  `ebisu_clock_t`'s time: a chain to a root in the Windows root store, valid
  for server authentication, issued for the host. It looks the host up with
  `GetAddrInfoExW()` and tries each of its addresses, IPv6 and IPv4, both
  within its connect timeout, and cancels a lookup the timeout cuts short. It
  drives Schannel itself, keeping what it has received but not yet returned,
  and waits for a receive with `select()`, so that a timeout leaves the
  connection usable, as its `recv_wait` waits too. Its `last_error` says why
  an operation failed, which also goes to the logger set with
  `ebisu_schannel_socket_set_logger()`, if any, unless it is built with
  `-DEBISU_LOG=OFF`. Needs Windows 8 or later, and only what it provides,
  `ws2_32`, `secur32` and `crypt32`, which it links.

- **FreeRTOS** (`ebisu_freertos.h`, library `ebisu_freertos`):
  `ebisu_freertos_system_t`, whose tasks are FreeRTOS tasks of the stack depth
  and priority it is initialized with, made with `xTaskCreate()`, whose delays
  are `vTaskDelay()`, for at least the time asked for, and whose monotonic
  clock is the tick count with the times it has overflowed, which doesn't
  wrap, and `ebisu_freertos_lock_t`, a FreeRTOS mutex, in the lock itself with
  `configSUPPORT_STATIC_ALLOCATION`, and waiting past `portMAX_DELAY` without
  `INCLUDE_vTaskSuspend`. Built where the application's CMake build has
  FreeRTOS-Kernel's own target, `freertos_kernel`, which adding the kernel's
  directory before ebisu's makes, with the application's `FreeRTOSConfig.h`,
  and not installed, as the kernel isn't. Needs FreeRTOS-Kernel V11.1 or
  later, that of FreeRTOS LTS 202406, `configUSE_MUTEXES` and
  `INCLUDE_vTaskDelay`, and for tasks `configSUPPORT_DYNAMIC_ALLOCATION` and
  `INCLUDE_vTaskDelete`, without which the system has none to make. jkii reads
  `errno` after `strtoll()`, and the handler's and the updater's tasks parse
  JSON at once, so each task needs its own: with newlib,
  `configUSE_NEWLIB_REENTRANT`.

- **mbedTLS** (`ebisu_mbedtls.h`, library `ebisu_mbedtls`):
  `ebisu_mbedtls_socket_t`, TLS 1.2 or later, 1.3 where mbedTLS has it, with
  mbedTLS 3.6 or 4.x, over another socket of the system's, of plain TCP, which
  connects, sends and receives the records, and whose timeouts are the
  connection's: the POSIX port's `ebisu_posix_tcp_socket_t`, the Win32 port's,
  or one over a microcontroller's TCP/IP stack. Its connections share an
  `ebisu_mbedtls_tls_t`: the CA certificates the server's has to chain to, PEM
  or DER, from memory, and mbedTLS's configuration, which
  `ebisu_mbedtls_tls_config()` gives the application for settings of its own,
  such as the cipher suites. It checks the server's certificate chain and host
  name, and each certificate's dates at an `ebisu_clock_t`'s time, in place of
  mbedTLS's own check at the system's, or at the system's time without one, as
  mbedTLS has it with `MBEDTLS_HAVE_TIME_DATE`: a device without a real-time
  clock connects with the clock alone. Random numbers come from PSA's
  generator, which `ebisu_mbedtls_tls_init()` starts, from mbedTLS's entropy
  sources. A receive that times out leaves the connection as it was, and
  `recv_wait` counts what mbedTLS has and hasn't returned. Its `last_error`
  says why an operation failed: what the socket under it said, for what failed
  there, a certificate refused, with mbedTLS's verify flags, or another TLS
  failure, with mbedTLS's error, which also goes to the logger set with
  `ebisu_mbedtls_socket_set_logger()`, if any, unless it is built with
  `-DEBISU_LOG=OFF`. Built where CMake finds mbedTLS: the application's,
  added before ebisu's, or one installed, as its CMake package has it, both
  `MbedTLS::mbedtls`. Sockets of a configuration on more than one task at once
  need mbedTLS built with `MBEDTLS_THREADING_C`.

The Linux sample uses the POSIX and OpenSSL ports, the Windows sample the Win32
and Schannel ones, and the FreeRTOS sample the FreeRTOS one, with a socket of
its own, mbedTLS over FreeRTOS-Plus-TCP. The ESP32 sample has implementations of
its own, under `tio`.

On FreeRTOS, in the application's `CMakeLists.txt`, as the FreeRTOS sample's:

```cmake
# FreeRTOS-Kernel's CMake asks for these: FreeRTOSConfig.h's directory, the
# port, and a heap.
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE config)
set(FREERTOS_PORT GCC_ARM_CM4F)
set(FREERTOS_HEAP 4)
add_subdirectory(FreeRTOS-Kernel)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(ebisu EXCLUDE_FROM_ALL)
target_link_libraries(firmware PRIVATE tio ebisu_freertos)
```

```c
#include <ebisu_freertos.h>
#include <tio.h>

static ebisu_freertos_system_t sys;
static ebisu_freertos_lock_t session_lock;

    /* In words of StackType_t: a task that makes TLS connections needs the
       most, for the handshake. */
    ebisu_freertos_system_init(&sys, 3072, tskIDLE_PRIORITY + 2);
    if (!ebisu_freertos_lock_init(&session_lock)) {
        /* Out of heap, without configSUPPORT_STATIC_ALLOCATION. */
    }
    tio_handler_set_system(&handler, &sys.base);
    kii_session_set_lock(&session, &session_lock.base);
```

CI runs the port's tests,
[tests/freertos](https://github.com/KiiPlatform/ebisu/blob/master/tests/freertos/README.md),
in QEMU, against the oldest kernel it supports and the latest.

With mbedTLS over the POSIX port's socket, and the CA certificates of the
application's:

```c
#include <ebisu_mbedtls.h>
#include <ebisu_posix.h>

static ebisu_mbedtls_tls_t tls;
static ebisu_posix_tcp_socket_t tcp;
static ebisu_mbedtls_socket_t sock;

    /* ca_pem with its NUL, and its size counting it. */
    int ret = ebisu_mbedtls_tls_init(&tls, (const unsigned char*)ca_pem, ca_pem_size);
    if (ret != 0) {
        /* mbedTLS's or PSA's error. */
    }
    ebisu_posix_tcp_socket_init(&tcp, 15, 15);
    ebisu_mbedtls_socket_init(&sock, &tcp.base, &tls);
    ebisu_mbedtls_socket_set_clock(&sock, &clock);
    tio_handler_set_http_socket(&handler, &sock.base);
```


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
- **To a file kept under a size** (`ebisu_file_logger_t`): the same lines, to a
  file at a path. When a line would take it past its size, the file is renamed
  to its path with `.1` after it, which replaces the one there, and a new one
  started, so that a device's flash holds at most twice the size, and the last
  of the log. The line is formatted before the lock it is given is taken, and
  written, counted and the file renamed under it. With
  `ebisu_file_logger_set_timestamps()`, each line starts with the system's time
  of day, in UTC to the millisecond, as RFC 3339 has it, as in
  `2026-11-15T07:46:05.123Z error: failed to connect to example.com.`, taken
  under the lock, so that a file's times are in the order of its lines.
- **To syslog** (`ebisu_syslog_logger_t`, on POSIX): `syslog()`, with a
  facility and the severity of the level. `openlog()` is the application's to
  call, if it wants to.
- **To a syslog server over UDP** (`ebisu_syslog_udp_logger_t`): a datagram for
  each message, formatted as RFC 5424 says, with the time to the millisecond,
  the host's name, the application's and the process's identifier, and sent as
  RFC 5426 does, to an IPv4 or IPv6 server looked up once, when the logger is
  initialized. The host's name can be another than the system's, such as a
  device's serial number, for a server to tell devices apart that all have
  the same. Sending never waits: what the socket can't take at once is
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
    /* NULL for the host's name, as gethostname() gives it. A server that can't
       be looked up leaves a logger that drops everything. */
    ebisu_syslog_udp_logger_init(&to_server, "logs.example.com", 514, NULL, "my-device", 3,
            EBISU_LOG_DEBUG);
    ebisu_log_fanout_init(&logger, loggers, 3);

    ebisu_openssl_socket_set_logger(&sock, &logger.base);
    ebisu_log(&logger.base, EBISU_LOG_INFO, "started, version %s", "1.0");
    ...
    ebisu_syslog_udp_logger_close(&to_server);
```

The loggers' levels and the fan-out's array are set before they are used, and
don't change. Core's `ebisu_log_level_filter_t` wraps any logger in a level that
can change while it logs, as from a server's configuration, which
`ebisu_log_level_parse()` reads the name of. Core's `ebisu_log_slot_t` is a place
in the array for a logger that can be put in, changed or taken out while tasks
log through it: a logger to a syslog server, changed for one to another server,
or with another host name, by initializing the new one, putting it in, and
closing the old one, which no message is going to once the change returns; or
taken out while the application doesn't want its logs sent.

```c
static ebisu_posix_lock_t lock;
static ebisu_log_slot_t remote;
static ebisu_log_level_filter_t filtered;
static ebisu_logger_t* loggers[] = { &to_stderr.base, &remote.base };
static ebisu_log_fanout_t all;
static ebisu_syslog_udp_logger_t servers[2];

    ebisu_posix_lock_init(&lock);
    ebisu_log_slot_init(&remote, &lock.base);
    ebisu_log_fanout_init(&all, loggers, 2);
    ebisu_log_level_filter_init(&filtered, &all.base, EBISU_LOG_INFO, &lock.base);

    /* Later, from any task. */
    ebisu_log_level_filter_set_level(&filtered, EBISU_LOG_DEBUG);
    ebisu_syslog_udp_logger_init(&servers[1], "logs.example.com", 514, host_name, "my-device",
            3, EBISU_LOG_DEBUG);
    ebisu_logger_t* old = ebisu_log_slot_set(&remote, &servers[1].base);
    if (old != NULL) {
        ebisu_syslog_udp_logger_close(&servers[0]);
    }
```

## DNS probe

`ebisu_dns_probe.h`, library `ebisu_dns_probe`, asks whether the name servers
answer before a host is looked up. `getaddrinfo()` can't be given a timeout:
when the servers don't answer, a lookup waits for as long as the C library
keeps asking them again, and so does the task that looked up: how long is
below.

The probe needs nothing of the system's itself: it asks the name servers an
`ebisu_dns_t` of core's lists, as that has them asked. The POSIX and Win32
ports have the system's:

- `ebisu_posix_dns()`: the servers of `/etc/resolv.conf`, as the C libraries
  read it, or 127.0.0.1 without one, asked over BSD sockets;
- `ebisu_win32_dns()`: those of the network adapters that are up, as
  `GetAdaptersAddresses()` lists them, but the placeholders `fec0:0:0:ffff::1`
  to `::3`, which Windows lists for an adapter without an IPv6 server, asked
  over Winsock. Without any, none is.

An application on another system, such as one with lwIP, implements the two
operations that matter, `servers` and `ask`, and the probe works there too.

`ebisu_dns_probe()` sends one query for the host to each of the first 3
servers in turn, until one answers, giving each the time it is given.
`ebisu_dns_probe_servers()` asks the servers it is given instead. Any answer
counts, that the name doesn't exist too: the probe tells only whether a server
answers. A host that needs no server, an address or localhost, needs no probe.

`ebisu_dns_probe_socket_t` probes before another socket connects, and fails the
connect if no server answers, with `EBISU_SOCK_ERROR_DNS`, rather than have
the other socket look the host up. The rest is the other socket's, which may be
any: the OpenSSL or Schannel port's, or the POSIX or Win32 port's of plain TCP.

```c
#include <ebisu_dns_probe.h>
#include <ebisu_openssl.h>
#include <ebisu_posix.h>

static ebisu_openssl_socket_t tls;
static ebisu_dns_probe_socket_t sock;

    ebisu_openssl_socket_init(&tls, 30, 30);
    /* Each server has 3 seconds to answer. */
    ebisu_dns_probe_socket_init(&sock, &tls.base, ebisu_posix_dns(), 3000);
    ebisu_dns_probe_socket_set_logger(&sock, &logger.base);
    kii_set_http_socket(&kii, &sock.base);
```

`ebisu_syslog_udp_logger_init()` looks its server up too, once: probing the
server's name first tells whether that lookup would wait.

The probe is for a device whose lookups all go to the name servers. A name
that `/etc/hosts` or another source has still needs a server to answer the
probe.

### How long a lookup waits

When the name servers don't answer, a lookup ends when the C library gives up
on them. The UDP logger's init waits that long, and the thread for a host and
port of the OpenSSL port, or of the POSIX port's plain socket, lasts that long. Timed with servers that never answer, and
`/etc/resolv.conf`'s default options:

| Servers                     | glibc 2.26, 2.39 | musl 1.1.24, 1.2.5 |
|-----------------------------|------------------|--------------------|
| 1                           | 10 s             | 5 s                |
| 3                           | 28 s             | 5 s                |
| 3, and 2 `search` domains   | 56 s             | 5 s                |

glibc asks the servers one after the other, and a search list makes it ask
them again. musl asks them all at once, and gives up after its timeout.

`options timeout:1 attempts:1` in `/etc/resolv.conf` makes the wait 1 second
for one server, with either library, for every program on the system. glibc
also reads options from the environment variable `RES_OPTIONS`, for one
program: `RES_OPTIONS="timeout:1 attempts:1"` makes three servers' wait 3
seconds. musl doesn't read it, and glibc ignores it for a program that runs
with more than its user's privileges: setuid, setgid, or given file
capabilities, such as `cap_net_bind_service`. A shorter timeout fails a lookup
whose server takes longer to answer, as one over a slow link can.

With another source after `dns` on the `hosts:` line of `/etc/nsswitch.conf`,
as `myhostname` or `files`, glibc goes on to it when the servers don't answer.
The lookup then fails as for a name that doesn't exist, `EAI_NONAME`, where
without one it fails with `EAI_AGAIN`.

On Windows Server 2025, a lookup whose one server never answered ended after
12 seconds, and failed as for a name that doesn't exist, `WSAHOST_NOT_FOUND`.
The connect timeout of the Schannel port, or of the Win32 port's plain socket,
cuts it short: the lookup is cancelled, and the connect fails with
`EBISU_SOCK_ERROR_TIMEOUT`.
