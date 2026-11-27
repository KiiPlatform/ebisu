# Upgrading

What an application, or the build that packages ebisu, has to change or check
when it moves to a newer release. Fixes and additions that ask nothing of an
application are left to each version's
[release notes](https://github.com/KiiPlatform/ebisu/releases).

## From 1.3 to 2.0

2.0 keeps 1.3's API: the same functions and types, with additions. Its structs
have 1.3's layout, and the size macros it adds, such as `KII_TOKEN_MAX_SIZE`,
name the sizes 1.3 had. It changes how ebisu is built and installed, what its
libraries export, and how a good part of it behaves, as below.

From 2.0 on:

- Every 2.0.x keeps the ABI of 2.0.0: a program built against one runs with
  another's libraries, unchanged.
- A 2.1 may change the ABI, as it may add to the structs an application
  allocates, and so has another soname, `.so.2.1`. It keeps 2.0's API: what
  builds against 2.0 builds against 2.1.

### Change: build from the repository's root

1.3 built each library from its own directory, `kii/` or `tio/`, as its own
CMake project, and had Makefiles to drive it. Those were the root `Makefile`
with `make build`, the Makefiles in `khc/`, `jkii/`, `kii/` and `tio/`, and
`tio/linux-sample/Makefile`. All of them are gone. 2.0 is one CMake project,
configured from the root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local
```

`kii/` and `tio/` can't be configured on their own any more. A project that
embeds ebisu adds the root with `add_subdirectory()` and links the `kii` or
`tio` target. The Linux sample builds with `-DEBISU_BUILD_SAMPLES=ON`, and
`CMakePresets.json` has the presets ebisu's own CI uses.

### Relink: what is installed, and where

- **The libraries.** `libkii` still holds kii, khc, jkii and jsmn, and `libtio`
  the whole stack, so linking `-lkii` or `-ltio` alone still works. 2.0 also
  builds and installs `libkhc` and `libjkii` of their own, and installs every
  library's public headers. Built with `-DBUILD_SHARED_LIBS=OFF`, they are
  static.
- **Where they go.** They are installed under CMake's `GNUInstallDirs`, where
  1.3 used a fixed `lib`: that is `lib64` on some distributions. Set
  `-DCMAKE_INSTALL_LIBDIR=lib` to keep 1.3's layout.
- **The soname.** 1.3's carried the whole version, as `libkii.so.1.3.2`, so
  every release needed a relink. 2.0's has the major and the minor:
  `libkii.so.2.0`, `libtio.so.2.0`, `libkhc.so.2.0` and `libjkii.so.2.0`. The
  files are `lib<name>.so.2.0.<patch>`, and `lib<name>.so` is the link to
  build against. Programs built against 1.3 have to be built again.
- **`ebisu_version.h`.** It is generated into the build directory and installed
  with the headers. 1.3 wrote it into each library's `include/` in the source
  tree. One left there by a 1.3 build isn't installed.

### Check: only the public API is exported

The libraries are compiled with hidden visibility, so they export what the
installed headers declare and nothing else. 1.3's `libkii` exported 203
symbols, 108 of which were in no installed header. An application that called
one of those, such as an internal `_kii_` or `_khc_` function, no longer
links. Use what the public headers declare.

### Check: where the version comes from

1.3 took its version, and the soname from it, from `git describe --tags`
wherever it was built. Now:

- **In ebisu's own clone, worktree or submodule**, it still comes from
  `git describe --tags`, and the build configures again when a commit, a
  checkout or a tag changes what that says.
- **From a source archive**, such as GitHub's tarball of a release, it is the
  release's version, where 1.3 had none to give.
- **Copied into another project's repository**, ebisu no longer takes that
  project's tags for its own: it builds as 0.0.0, with a warning saying why.
  To give it a version there, commit a `version/VERSION` file.
- **`version/VERSION`**, when there is one, says the version over git and an
  archive. It holds one line, the version as `2.0.0`, which may have a `v`
  before it and a suffix such as `-rc.1` after it. Anything else fails the
  configure. ebisu's repository has none.

### Check: compiler flags

- **Your flags are kept.** 1.3's components overwrote `CMAKE_C_FLAGS`, so
  flags from a toolchain file, a packager or the command line were dropped.
  2.0 keeps them, and adds its own per target.
- **Release builds define `NDEBUG`.** A release build uses CMake's flags, as
  `-O3 -DNDEBUG`, where 1.3 replaced them with `-O2` alone. `assert()` is
  therefore compiled out of a release build.
- **Warnings are errors.** They are by default, with GCC and Clang. If a
  compiler newer than ebisu's own CI's warns where it didn't, configure with
  `-DEBISU_WERROR=OFF` rather than patch the source.
- **Tracing is off by default.** `KHC_TRACE` and `TIO_TRACE` are off by
  default, where 1.3 had `KHC_TRACE` on. They print every request and
  response, tokens and passwords included. Turn them on with `-DKHC_TRACE=ON`
  or `-DTIO_TRACE=ON` only where that output stays private. `TIO_TRACE` now
  works: in 1.3 nothing could turn it on, because tio's `CMakeLists.txt`
  declared `KHC_TRACE` twice instead.
- **Strict C99.** The libraries build as strict C99, `-std=c99
  -pedantic-errors`, or `-pedantic` with `-DEBISU_WERROR=OFF`. 1.3 used
  `-std=c99` alone. A local patch to ebisu that uses C11 or GNU extensions
  doesn't build.

### Check: new error codes

Each is appended after the last existing code, so every other code keeps its
value. A `switch` that lists every code, or a table indexed by code, needs the
new ones:

- `KHC_ERR_INVALID_RESPONSE`, `KII_ERR_INVALID_RESPONSE` and
  `TIO_ERR_INVALID_RESPONSE`: the response isn't HTTP that can be read. Its
  status line isn't one, or its `Content-Length`, `Transfer-Encoding` or chunks
  don't say where its body ends. 1.3 read such a response as best it could.
- `KII_ERR_CREATE_TASK`: `kii_start_push_task()` had no task-create callback,
  or the callback failed. 1.3 called the callback without checking it, and
  returned `KII_ERR_OK` whatever happened.

Some codes are no longer returned:

- `KHC_ERR_SOCK_CLOSE`, `KII_ERR_SOCK_CLOSE` and `TIO_ERR_SOCK_CLOSE`, because a
  failed close no longer changes a request's result. A request whose response
  was read whole succeeds, whatever the close callback returns.
- `TIO_ERR_WRITE_CALLBACK`, because a response too large for the buffer is now
  `TIO_ERR_TOO_LARGE_DATA`.

### Check: what requests are refused before they are sent

1.3 sent whatever it was given, which could break the request line or the
headers, or send a request other than the one asked for:

- `khc_set_host()`, `khc_set_path()` and `khc_set_method()` return
  `KHC_ERR_FAIL` for a space or a control character. One too long for khc is
  still `KHC_ERR_TOO_LARGE_DATA`.
- `khc_perform()` returns `KHC_ERR_FAIL`, without connecting, for a header with
  a CR or an LF in it, or when a socket callback, the write callback or the
  header callback isn't set.
- `khc_perform()` returns `KHC_ERR_FAIL`, without connecting, for a response
  header buffer or a stream buffer set with a size of 0, as through
  `kii_set_resp_header_buff()` and `kii_set_stream_buff()`. 1.3 didn't bound
  reads into a header buffer of size 0 by it, and ended a body early with a
  stream buffer of size 0. A NULL buffer is still allocated.
- `kii_api_call_append_header()` returns `KII_ERR_FAIL` for a name or a value
  with a CR or an LF in it.
- Every kii call fails, rather than send its request without a path, when the
  path, or the host, is too long or holds what would break the request. 1.3's
  kii didn't check what khc returned. This
  applies to a path built from the application's bucket, object, topic or
  server code names. The call returns `KII_ERR_TOO_LARGE_DATA` or `KII_ERR_FAIL`.
- A request whose path would hold an ID or a name the application gave, or the
  thing's own ID, of anything but letters, digits, `.`, `_`, `-` and `$`, or
  one that is empty, `.` or `..`, fails with `KII_ERR_FAIL`, sending nothing.
  That is bucket, object and topic names, scope IDs, an installation ID, and a
  server code version or endpoint name. Every request also fails so with an
  application ID of anything but letters and digits, or none. Kii's IDs and
  names are made of those characters. 1.3 put whatever it was given into the
  path, and a `/`, `?` or `#` in one sent the request to another path.
- tio refuses a command with `TIO_ERR_FAIL`, running none of its actions, when
  the thing's ID can't be a segment of its results' path.
- GET, HEAD and DELETE requests send no body. A body set for one with
  `kii_api_call_append_body()` is no longer sent.

### Check: responses and the buffer

- **A response ends where its framing says.** That is after its
  `Content-Length`, after the last chunk and its trailers, or, for HEAD, 204
  and 304, with its headers. 1.3 read until the server closed the
  connection. A connection that closes before the response ends is now
  `KHC_ERR_SOCK_RECV` (`KII_ERR_SOCK_RECV`, `TIO_ERR_SOCK_RECV`). 1.3 took
  what had come as the whole of it.
- **Too large for the buffer.** A response too large for the buffer set by
  `kii_set_buff()` is `KII_ERR_TOO_LARGE_DATA`; 1.3 gave
  `KII_ERR_WRITE_CALLBACK`. The body can be at most one byte shorter than the
  buffer, because kii keeps a NUL after it. So a body that filled the buffer
  exactly, which 1.3 took, now fails. Read the body by
  `kii_get_resp_body_length()`: it isn't a string, as it may hold a NUL of its
  own.
- **Each request starts afresh.** `khc_perform()` resets what the last request
  and its response left behind, as `khc_reset_except_cb()` does, which also
  clears the request. 1.3 read the next response with the last one's state
  unless `khc_reset_except_cb()` was called in between, and could end its body
  early with `KHC_ERR_OK`. A request that fails before reaching the server no
  longer keeps the status code of the one before it. kii resets khc before
  each request, so this concerns an application calling khc itself.
- **A write callback that returns more than it was given** fails the request
  with `KHC_ERR_WRITE_CALLBACK`, as `KHC_CB_WRITE` says, wherever khc calls
  it. 1.3 failed it only for a body's first part, read with the headers, and
  took it for the rest. kii's own callback returns what it was given.
- **ETags.** `kii_get_etag()` gives the last response's ETag until another
  response comes, and is empty when that response had none. It can be passed
  straight to `kii_put_object()` or `kii_patch_object()`. A request that gets
  no response, as when it can't connect, leaves it, so the request tried again
  sends the same `If-Match`. 1.3 emptied it as each call started, so a call
  tried again went without one.
- **Downloads.** `kii_download_object_body()`'s callback gets the object's body
  alone. The body of a response other than 2xx goes into kii's buffer instead,
  as for calls without a callback. 1.3 wrote an error's JSON into the
  download too.
- **Logins.** `kii_auth_thing()`, thing registration and onboarding leave the
  author as it was when their response can't be read whole. 1.3 could leave half of it filled in.
- **Firmware version and properties.** `kii_ti_put_firmware_version()` escapes
  the version for JSON, so pass it unescaped. `kii_ti_onboard()` sends the
  thing's properties as given, where 1.3 escaped them, so pass them as a
  JSON object.
- **MQTT endpoint.** An endpoint whose response has no `X-MQTT-TTL`, which is
  obsolete, is taken. 1.3 refused it.

### Check: the push task, the handler and the updater

- **Starting `kii_start_push_task()` and `tio_handler_start()`.** They fail
  with `KII_ERR_FAIL` or `TIO_ERR_FAIL`, without starting a task, when:
  - a task is already running on the `kii_t` or the handler;
  - a socket callback, the delay callback, the buffer or the MQTT buffer isn't
    set;
  - the push callback is NULL, or, for the handler, the author or the action
    callback is;
  - the keep-alive is above 65535, the largest MQTT carries.

  A missing or failing task-create callback is `KII_ERR_CREATE_TASK` or
  `TIO_ERR_CREATE_TASK`. 1.3 checked none of this: it called the task-create
  callback whatever was set, and two tasks could share one socket and buffer.
- **Starting `tio_updater_start()`.** It fails the same way when its task is
  already running, or when the author, a callback, a socket callback or the
  buffer is missing. It also fails with `TIO_ERR_FAIL` when no interval is set
  with `tio_updater_set_interval()`: 0, `tio_updater_init()`'s value, made
  1.3's updater upload as fast as it could.
- **Stopping the updater.** Its task-continue callback is asked once a second
  while it waits, so returning false stops it within about a second, whatever
  the interval.
- **The MQTT connection at exit.** When the push task ends, it closes its
  connection with the MQTT close callback before calling the task-exit callback
  (`kii_set_cb_task_exit()`, `tio_handler_set_cb_task_exit()`). A task-exit
  callback that closed the MQTT socket itself would now close it twice: leave
  that to ebisu, and free only the buffers there.
- **Reconnecting.** The push task reconnects on what it can't read: a packet
  cut off, one of the wrong length, the stream ending, or a receive failure. It
  fetches new MQTT credentials when the broker refuses them, and goes on when
  the network fails as it starts. 1.3 could end, spin or misread the stream
  in each of those cases.
- **A dead connection is found.** It is detected within one keep-alive
  interval, from a PINGREQ the broker didn't answer, and the push task
  reconnects. 1.3 noticed only when sending failed, which on a half-open
  connection can take many minutes.
- **A packet across reads is read whole.** One that came in more than one read
  is no longer overwritten by its own rest.

### Check: commands and their results

- **A malformed command is refused whole.** `tio_handler_handle_command()` and
  the push handler return `TIO_ERR_PARSE_JSON` before the action callback is
  called for any of the command's actions, and send no results. 1.3 ran the
  actions before the malformed part and dropped those after it. Malformed means
  any of:
  - an alias or an action that isn't an object;
  - actions that aren't an array;
  - an empty action object;
  - a value jkii can't read;
  - a command ID that can't be a segment of its results' path.
- **A long action name** is `TIO_ERR_TOO_LARGE_DATA` when the results buffer
  can't hold it. 1.3 copied it onto the stack, whatever its length.
- **Results reach the server.** They are valid JSON for every alias, and are
  written whatever their length. 1.3's results for a command with more than
  one alias were invalid JSON, which the server refused. A refused results
  request, or one that couldn't be opened, is now reported as an error.

### Check: JSON parsing (jkii)

- **Refused as `JKII_ERR_INVALID_INPUT`.** These are now refused, where 1.3
  asserted or misread them:
  - empty input;
  - a number without digits;
  - an integer with a leading zero, which 1.3 read as octal;
  - anything else jsmn's strict grammar refuses.

  A trailing comma is still taken. When the input can't be read, every field
  is set to `JKII_FIELD_ERR_NOT_FOUND`.
- **`null`, `true` and `false` whole.** A primitive that is only the start of
  one, as `n`, is no longer taken for the whole word, as 1.3 did.
- **Integer types.** An integer's type follows its value:
  `JKII_FIELD_TYPE_INTEGER` if it fits in an int, `JKII_FIELD_TYPE_LONG` if it
  fits only in a long, and an overflow or underflow otherwise. 1.3 decided by
  the number of characters.
- **Doubles.** A whole number such as `25` is read for a field asked for as a
  double. Doubles are read whatever the locale's decimal point.
- **Lookups.** A path that isn't valid, or a key without quotes, is not found.
  Names are looked up only in objects.
- **Field buffers.** `jkii_parse()` never writes a number or a boolean over a
  field's buffer address. A field with a buffer that finds one is
  `JKII_FIELD_ERR_COPY`, or, expected as `JKII_FIELD_TYPE_ANY`, found without
  its value.
- **Escaping.** `jkii_escape_str()` escapes control characters other than `\b`,
  `\f`, `\n`, `\r` and `\t` as `\u00XX`. That can make the escaped string up to
  six times longer, so size its buffer by `strlen(str) * 6 + 1`.

### Check: the Linux sample

For applications that started from it:

- it ignores SIGPIPE;
- it detaches its task threads;
- it sleeps the whole delay asked for;
- it locks OpenSSL 1.0.2 for its threads;
- it reads OpenSSL's errors as OpenSSL asks to be;
- it doesn't shut down a connection that failed;
- it no longer prints the thing's password;
- it verifies the server's certificate, against the CA certificates the
  system trusts, or those `SSL_CERT_FILE` or `SSL_CERT_DIR` names, and that
  the certificate is for the host it connects to, which it tells the server;
- it takes TLS 1.2 or later;
- it looks hosts up with `getaddrinfo()`, safe in tasks that connect at the
  same time, and connects to their IPv6 addresses too;
- its sockets aren't inherited by programs the application runs;
- its receives, sends and handshakes wait on through signals, until their
  timeouts run out;
- it closes the socket, and frees what it made, when a connect fails;
- it ends on SIGTERM as on Ctrl-C, and exits when a task can't be started,
  rather than waiting for it for ever.

An application with a copy of it may want the same changes.
