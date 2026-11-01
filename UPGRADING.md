# Upgrading

What an application has to change or check when it moves to a new major version
of ebisu. Fixes and additions that ask nothing of an application are left to
each version's [release notes](https://github.com/KiiPlatform/ebisu/releases).

## Breaking changes in 3.0

Compared with 2.0.0. 3.0 has not been released yet, so this section follows
`master`.

For most applications this is two edits and a rebuild. v2.0.0's Linux sample
builds and links against this code once its connect callback is changed and its
call to a setter that did nothing is deleted. Those setters are the only
functions of 2.0.0 that were removed, and no constant or public struct member
was.

### Code that stops compiling

**The connect callback takes its port as a `uint16_t`.** Every application
implements `KHC_CB_SOCK_CONNECT`:

```c
// 2.0
khc_sock_code_t cb_connect(void* sock_ctx, const char* host, unsigned int port);
// 3.0
khc_sock_code_t cb_connect(void* sock_ctx, const char* host, uint16_t port);
```

It is what `khc_set_cb_sock_connect()`, `kii_set_cb_http_sock_connect()`,
`kii_set_cb_mqtt_sock_connect()`, `tio_handler_set_cb_sock_connect_http()`,
`tio_handler_set_cb_sock_connect_mqtt()` and
`tio_updater_set_cb_sock_connect()` take.

**The delay callback takes its milliseconds as a `uint32_t`.** This is
`KII_CB_DELAY_MS`, which `kii_set_cb_delay_ms()`,
`tio_handler_set_cb_delay_ms()` and `tio_updater_set_cb_delay_ms()` take:

```c
// 2.0
void cb_delay_ms(unsigned int msec, void* userdata);
// 3.0
void cb_delay_ms(uint32_t msec, void* userdata);
```

Where `uint32_t` is `unsigned int`, as on x86, the old declaration is still the
same type. With arm-none-eabi GCC and newlib, `uint32_t` is `unsigned long`, so
it no longer matches.

Either mismatch is an error in C++, and in C with GCC 14 or later. Older C
compilers only warn, with `-Wincompatible-pointer-types`. Change the callback
all the same: calling a function through the wrong type is undefined.

**`kii_set_mqtt_to_sock_send()` and `tio_handler_set_mqtt_to_sock_send()` are
gone.** Delete the call. They stored a value nothing read, in 2.0 as well, so
this changes nothing at run time. An MQTT socket still needs a send timeout,
which is the application's to set in its socket callbacks, as the samples do.

### Values whose type changed

These still compile. Check anything that depends on their width: format
strings, pointers to them, and narrower variables they are stored in.

| Where | What | 2.0 | 3.0 |
| --- | --- | --- | --- |
| `khc_get_status_code()`, `kii_get_resp_status()` | result | `int` | `uint16_t` |
| `khc_method_can_have_body()` | result | `int` | `bool` |
| `khc_enable_insecure()` | `enable_insecure` | `int` | `bool` |
| `kii_start_push_task()` | keep-alive | `unsigned int` | `uint32_t` |
| `kii_set_mqtt_to_sock_recv()`, `tio_handler_set_mqtt_to_sock_recv()` | timeout | `unsigned int` | `uint32_t` |
| `tio_handler_set_keep_alive_interval()`, `tio_updater_set_interval()` | interval | `size_t` | `uint32_t` |
| `jkii_field_t`, `jkii_primitive_t` | `int_value` | `int` | `int32_t` |
| `jkii_field_t`, `jkii_primitive_t`, `tio_action_value_t` | `long_value` | `long` | `int64_t` |
| `kii_mqtt_endpoint_t` | `port_tcp`, `port_ssl` | `unsigned int` | `uint16_t` |
| `kii_mqtt_endpoint_t` | `ttl` | `unsigned long` | `uint32_t` |

Print the fixed-width ones with `PRId32`, `PRId64` and `PRIu32`, or a cast. A
`size_t` passed for an interval is converted at the call, so on 64-bit hosts a
value above `UINT32_MAX` is cut down before anything can check it.

### Rebuild everything

- The layouts of `khc`, `kii_t`, `kii_author_t`, `kii_mqtt_endpoint_t`,
  `tio_handler_t` and `tio_updater_t` changed. Code built against 2.0's headers
  can't use 3.0's libraries.
- `libkii` and `libtio` move to `.so.3`. `libkhc` and `libjkii` had no version
  in their soname, and now have one, `libkhc.so.3` and `libjkii.so.3`, with the
  same files and links as `libkii`.
- `kii_t`, `tio_handler_t` and `tio_updater_t` are about 700 bytes larger on
  x86-64, and `kii_author_t` goes from 256 to 384 bytes with `refresh_token`.
  Check any static memory or task stack that holds one.
- A device that saved a `kii_author_t` as bytes can read 2.0's into a zeroed
  3.0 one: `author_id` and `access_token` keep their offsets, and `refresh_token`
  comes after them.

### Building

Without CMake, as a vendor's build usually does:

- Add `khc/src/khc_http_date.c`, `kii/kii_clock.c` and `kii/kii_session.c` to
  the sources.
- The sources now include a generated `ebisu_version.h`. Write it with
  `version/ebisu_version.sh <output>` and put it on the include path, as the
  Makefiles of the embedded samples do.
- Under `ERRNO_HACK_NEEDED`, `jkii_utils.h` no longer defines `errno`, `EINVAL`
  and `ERANGE`. Code that took them from there has to define its own.

With CMake:

- CMake 3.15 is the minimum, up from 3.12. The presets need 3.21.
- `KHC_TRACE` and `TIO_TRACE` are off by default. Pass `-DKHC_TRACE=ON` or
  `-DTIO_TRACE=ON` to get the trace back.

### Behaviour changes

All of this compiles unchanged. Most of it is ebisu reporting what it used to
hide, so an application can meet errors it has never had to handle.

Starting tasks:

- `tio_updater_start()` returns `TIO_ERR_FAIL` until `tio_updater_set_interval()`
  has set an interval above 0. 2.0 uploaded as fast as it could instead.
- `kii_start_push_task()` and `tio_handler_start()` refuse a keep-alive above
  65535, with `KII_ERR_FAIL` and `TIO_ERR_FAIL`. 2.0 sent the broker the value
  cut down to 16 bits.
- The result of the task-create callback now counts, so it has to return
  `KII_TASKC_OK` when it succeeds. Anything else makes `kii_start_push_task()`
  return the new `KII_ERR_CREATE_TASK` and `tio_handler_start()` return
  `TIO_ERR_CREATE_TASK`, where 2.0 ignored it and reported success. A callback
  that was never set gives the same codes, and `TIO_ERR_CREATE_TASK` from
  `tio_updater_start()`.

- `kii_start_push_task()`, `tio_handler_start()` and `tio_updater_start()` return
  `KII_ERR_FAIL` or `TIO_ERR_FAIL` if something the task uses isn't set: the
  callback they are given, or for tio the state reader, the delay callback, a
  socket callback or a buffer. 2.0 started a task that crashed on reaching it.
- They return the same while the task of an earlier start is still running,
  where 2.0 started a second one on the same sockets and buffers. It stops
  running just before its exit callback, which can start it again. A task ended
  any other way, such as by killing its thread, needs `kii_init()`,
  `tio_handler_init()` or `tio_updater_init()` before starting again.

The push task:

- It closes its MQTT connection before calling the exit callback. An exit
  callback that closes it too now closes it twice, so it should only free what
  it owns.
- A network failure while installing push or fetching the endpoint is tried
  again, where 2.0 ended the task. An application no longer sees the exit
  callback for these, and needn't restart the handler.
- Failed attempts back off, from about a second up to five minutes, where 2.0
  tried every second or two. A device can take up to five minutes to come back
  after a long outage. Reaching a subscribed connection starts the waits again.
  After 8 failures in a row the task asks for the endpoint again.

Commands in tio:

- Results the server refuses are reported as `TIO_ERR_RESP_STATUS`, through the
  error callback and from `tio_handler_handle_command()`. 2.0 reported nothing.
  The action has already run by then.
- A command with an empty alias or action object, or with an action value jkii
  can't parse, is `TIO_ERR_PARSE_JSON` and doesn't reach the action callback.

kii:

- `kii_get_mqtt_endpoint()` fails on a port outside 0 to 65535, or a TTL that is
  negative or above `UINT32_MAX`.
- `kii_ti_put_firmware_version()` escapes the version, as onboarding always has.
  A caller that escaped it first should stop.

khc, which kii and tio report in their own codes:

- A connection that closes before the response ends is `KHC_ERR_SOCK_RECV`. In
  2.0, `khc_perform()` didn't return if that happened during the headers, and
  returned `KHC_ERR_OK` short of a `Content-Length` and `KHC_ERR_FAIL` in a
  chunked body.
- An invalid `Content-Length`, or a `Transfer-Encoding` whose final coding isn't
  `chunked`, is `KHC_ERR_HEADER_CALLBACK`, before any of the body is written.
- An invalid chunk size, or a first line that isn't an HTTP status line, is
  `KHC_ERR_FAIL`.
- A socket that fails to close no longer changes a request's result: a response
  read whole is `KHC_ERR_OK`, and a request that failed keeps its failure. In
  2.0 it was `KHC_ERR_SOCK_CLOSE`, which kii and tio reported as a failure to
  connect. That code, `KII_ERR_SOCK_CLOSE` and `TIO_ERR_SOCK_CLOSE` stay defined
  but aren't returned, and `khc_get_close_failed()` says whether it happened.
- GET, HEAD and DELETE send no body, and don't call the read callback.

jkii:

- An integer's type follows its value: `int32_t`, then `int64_t`, then overflow
  or underflow, whatever its length. 2.0 went by the number of characters, so
  `2147483648` could be an overflow, and very long integers were doubles. A
  double field given one now reports a type mismatch.
- Integers with leading zeros are invalid, path indices such as `/[010]`
  included. 2.0 read them as octal. `0` and `-0` are still numbers.
- Numbers without digits, such as `-`, `1e` and `1e+`, are invalid.
- Empty or whitespace-only input is `JKII_ERR_INVALID_INPUT`.
- Fields looked up by name are found only in an object. In an array, 2.0 took
  the elements for pairs of a name and a value. Fields looked up by path still
  reach an array's elements.
- A path lookup doesn't find a key that isn't a string, or follow a path that
  isn't valid.
- `jkii_escape_str()` can write six bytes for each one it reads, up from two. It
  returns -1 if the buffer is too small, so size one for it at
  `strlen(str) * 6 + 1`.

### If you start using sessions

`kii_session_t` is new and optional. Without one, tokens are handled as in 2.0.
With one:

- A `TIO_CB_SIZE` callback has to restart its `TIO_CB_READ` reader whenever it is
  called, as the updater uploads again after a token is renewed.
- A renewal keeps a copy of `khc` on the stack of the task making the request,
  about 700 bytes on x86-64. Allow for it in that task's stack.
