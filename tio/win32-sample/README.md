# TIO sample application on Windows

The [linux sample](../linux-sample/README.mkd) on the Win32 API, with the same
tasks, callbacks and command line. It needs nothing beyond what Windows
provides:

| | linux sample | win32 sample |
|---|---|---|
| sockets | BSD sockets | Winsock 2 |
| TLS | OpenSSL | Schannel, through SSPI, and crypt32 to check the certificate |
| tasks | pthreads | `_beginthreadex()` |
| Ctrl-C | `sigaction()` | `SetConsoleCtrlHandler()` |

## Build

It is built with mingw-w64, cross-compiling from Linux, and run under wine.
From the root of the repository:

```sh
cmake --preset mingw
cmake --build --preset mingw
```

This makes `build/mingw/tio/win32-sample/win32app.exe`, with `libtio.dll` and
`state.json` beside it. The preset needs mingw-w64 (`mingw64-gcc` and
`mingw64-gcc-c++` on Fedora, `g++-mingw-w64-x86-64-posix` on Debian and
Ubuntu) and wine.

The sample is built whenever `EBISU_BUILD_SAMPLES` is on and the target is
Windows, where the linux sample is not. It avoids GNU extensions in its own
code and links the Windows libraries by name, so a native MinGW or MSVC build
on Windows should work too, but only mingw-w64 and wine are tested.

## Use

Set up the application, thing type, trait, alias and owner on
https://developer.kii.com as the [linux sample's
README](../linux-sample/README.mkd) describes, and put your application's ID
and site in `KII_APP_ID` and `KII_APP_HOST` in `example.h`.

Then, from the directory `win32app.exe` is in (prefix the command with `wine`
on Linux):

```sh
win32app onboard --vendor-thing-id={vendor thing id} --password={password}
```

`--vendor-thing-id {id}` and `--password {password}`, with a space, work too.

The sample onboards the thing, then prints the actions of each command it
receives, and uploads `state.json`, from the directory it runs in, every
`UPDATE_PERIOD_SEC` seconds. Ctrl-C stops it, once both tasks have noticed:
the command task waits for the MQTT broker for up to `TO_RECV_SEC` seconds at a
time.

## TLS

`sock_cb_win32.c` drives Schannel itself, and keeps what it has received but
not yet returned between calls: TLS records come apart and together in any
pieces TCP likes, and the MQTT task reads one byte at a time.

- The handshake sends the host name as SNI. The protocol versions are those
  the system enables; with `SCHANNEL_CRED` that is TLS 1.2 at most.
- The server certificate must chain to a root in the Windows root store, be
  issued for the host name and be in date. Revocation is not checked.
  Under wine the root store is the CA bundle of the system wine runs on.
- A receive that times out returns `KHC_SOCK_FAIL`, and
  `sock_cb_recv_timed_out()` tells the MQTT task it was a timeout, so it waits
  on instead of connecting again. The wait is a `select()`, rather than
  `SO_RCVTIMEO`, so that no `recv()` is ever cut short by the timeout: Winsock
  documents that as leaving the socket in an indeterminate state.
- The end of the stream, `KHC_SOCK_OK` with 0 bytes, is a close_notify or the
  end of the TCP stream between records; a reset, or an end in the middle of a
  record, is a failure.
- Renegotiation is not supported: a server that asks for it gets a failed
  receive.

Wine's Schannel differs from Windows' in two ways the sample deals with: it
accepts any certificate, so the sample validates with crypt32, which wine
implements, rather than leave it to Schannel; and it reports a close_notify as
an empty record rather than `SEC_I_CONTEXT_EXPIRED`.

Under wine, every certificate refused with `0x80096004`
(`TRUST_E_CERT_SIGNATURE`), valid ones included, means a wine prefix whose
setup was cut short, so that crypt32 has no RSA provider to check signatures
with: one made by `wineboot` in a container build step, say, which ended before
`wineserver` had saved it. `wineserver -w` after `wineboot` waits for that.

## Test

`ctest --preset small-mingw` includes `win32app_sock_cb`, which runs
`test/tls_test.py`: `test/sock_cb_test.c` calls the socket callbacks against
TLS servers on 127.0.0.1 whose ciphertext the script controls, covering large
and fragmented records, timeouts, resets, close_notify and refused
certificates. It needs Python 3.9 or later and the `openssl` command, makes a
wine prefix of its own under the build directory and removes it afterwards.
