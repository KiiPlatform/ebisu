# Upgrading

What an application has to change or check when it moves to a new major version
of ebisu. Fixes and additions that ask nothing of an application are left to
each version's [release notes](https://github.com/KiiPlatform/ebisu/releases).

## Breaking changes in 3.0

Compared with 1.3, the last 1.x release line. v2.0.0 was tagged on the way, and
what it changed is included here. 3.0 has not been released yet, so this section
follows `master`.

Every application changes how it gives ebisu its sockets and its system, and
most then have one more edit and a rebuild.

### Code that stops compiling

**Sockets and the system are objects, set with one call each.** In 1.x each
socket and task callback was set on its own, with a context pointer of its own.
In 3.0 a socket is an `ebisu_socket_t` and the system an `ebisu_system_t`, each
a pointer to a table of operations, in
[core](https://github.com/KiiPlatform/ebisu/blob/master/core/README.md).
Each operation gets the object as its first argument, in place of the context
pointer: embed the object in the struct that was the context, and get that back
with `EBISU_CONTAINER_OF()`.

```c
// 1.x
khc_sock_code_t cb_connect(void* sock_ctx, const char* host, unsigned int port)
{
    socket_context_t* ctx = (socket_context_t*)sock_ctx;
    ...
}
...
    tio_handler_set_cb_sock_connect_http(handler, cb_connect, &http_ctx);
    tio_handler_set_cb_sock_send_http(handler, cb_send, &http_ctx);
    tio_handler_set_cb_sock_recv_http(handler, cb_recv, &http_ctx);
    tio_handler_set_cb_sock_close_http(handler, cb_close, &http_ctx);

// 3.0, with ebisu_socket_t base as a member of socket_context_t
ebisu_sock_code_t cb_connect(ebisu_socket_t* self, const char* host, uint16_t port)
{
    socket_context_t* ctx = EBISU_CONTAINER_OF(self, socket_context_t, base);
    ...
}

static const ebisu_socket_ops_t socket_ops = {
    cb_connect, cb_send, cb_recv, cb_close, NULL
};
...
    http_ctx.base.ops = &socket_ops;
    tio_handler_set_http_socket(handler, &http_ctx.base);
```

ebisu keeps a pointer to each object, so it has to outlive what it is set on.
The samples' `sock_cb_init_context()` and system functions show the whole of it.
Initialize a table whole, as above: an operation it leaves out, such as the
optional `last_error` after `recv_timed_out`, is then `NULL`, where one assigned
field by field would be whatever the memory held.

| 1.x | 3.0 |
|---|---|
| `khc_set_cb_sock_connect()`, `_send()`, `_recv()` and `_close()` | `khc_set_socket()` |
| `kii_set_cb_http_sock_connect()`, `_send()`, `_recv()` and `_close()` | `kii_set_http_socket()` |
| `kii_set_cb_mqtt_sock_connect()`, `_send()`, `_recv()` and `_close()` | `kii_set_mqtt_socket()` |
| `tio_handler_set_cb_sock_connect_http()`, `_send_http()`, `_recv_http()` and `_close_http()` | `tio_handler_set_http_socket()` |
| `tio_handler_set_cb_sock_connect_mqtt()`, `_send_mqtt()`, `_recv_mqtt()` and `_close_mqtt()` | `tio_handler_set_mqtt_socket()` |
| `tio_updater_set_cb_sock_connect()`, `_send()`, `_recv()` and `_close()` | `tio_updater_set_socket()` |
| `kii_set_cb_task_create()` and `kii_set_cb_delay_ms()` | `kii_set_system()` |
| `tio_handler_set_cb_task_create()` and `tio_handler_set_cb_delay_ms()` | `tio_handler_set_system()` |
| `tio_updater_set_cb_task_create()` and `tio_updater_set_cb_delay_ms()` | `tio_updater_set_system()` |

The operations' types replace the callback types `KHC_CB_SOCK_CONNECT`,
`KHC_CB_SOCK_SEND`, `KHC_CB_SOCK_RECV`, `KHC_CB_SOCK_CLOSE`,
`KII_CB_TASK_CREATE` and `KII_CB_DELAY_MS`, which are gone. Two arguments
changed type besides: the connect operation takes its port as a `uint16_t`, and
the delay its milliseconds as a `uint32_t`, which with arm-none-eabi GCC and
newlib is `unsigned long`. A function of the wrong type in an operations table is
an error in C++, and in C with GCC 14 or later. Older C compilers only warn, with
`-Wincompatible-pointer-types`: fix it all the same, as calling a function
through the wrong type is undefined.

The rest moved to `core` and was renamed, with no aliases for the old names:

| 1.x | 3.0 |
|---|---|
| `khc_socket_callback.h` | `ebisu_socket.h` |
| `khc_sock_code_t` | `ebisu_sock_code_t` |
| `KHC_SOCK_OK`, `KHC_SOCK_FAIL`, `KHC_SOCK_AGAIN` | `EBISU_SOCK_OK`, `EBISU_SOCK_FAIL`, `EBISU_SOCK_AGAIN` |
| `kii_task_code_t` | `ebisu_task_code_t` |
| `KII_TASKC_OK`, `KII_TASKC_FAIL` | `EBISU_TASKC_OK`, `EBISU_TASKC_FAIL` |
| `KII_TASK_ENTRY` | `EBISU_TASK_ENTRY` |

`kii_task_callback.h` remains for `KII_CB_TASK_EXIT` and
`KII_CB_TASK_CONTINUE`, which are still set with a function and a context
pointer each. The types above are in `ebisu_system.h`, which `kii.h` includes.

**`kii_bool_t` and `tio_bool_t` are gone, for C99's `bool`.** khc and core
take `bool` already, and `kii.h` and `tio.h` include `stdbool.h`. Every callback
returning one, the task continue callback and tio's action and push callbacks,
now returns `bool`, and every function taking or returning one, and every field,
is `bool`. Replace the names, with no aliases for them:

| 1.x | 3.0 |
|---|---|
| `kii_bool_t`, `tio_bool_t` | `bool` |
| `KII_TRUE` | `true` |
| `KII_FALSE` | `false` |

With GNU sed, for each source file:

```
sed -i -e 's/\b\(kii\|tio\)_bool_t\b/bool/g' \
    -e 's/\bKII_TRUE\b/true/g' -e 's/\bKII_FALSE\b/false/g' app.c
```

A comparison such as `== KII_TRUE` becomes `== true`, which means the same for a
`bool`.

**jkii's tokens are gone.** jkii parses with coreJSON, which needs no token
array, so `jkii_token_t`, `jkii_resource_t`, `JKII_CB_RESOURCE_ALLOC` and
`JKII_CB_RESOURCE_FREE` are gone, and what took them:

| 1.x | 3.0 |
|---|---|
| `jkii_parse(json, len, fields, &resource)` | `jkii_parse(json, len, fields)` |
| `jkii_validate_root_object(json, len, &resource)` | `jkii_validate_root_object(json, len)` |
| `jkii_parse_with_allocator()`, `jkii_validate_root_object_with_allocator()` | `jkii_parse()`, `jkii_validate_root_object()` |
| `kii_set_json_parser_resource()`, `kii_set_cb_json_parser_resource()` | nothing: delete the call |
| `tio_handler_set_json_parser_resource()`, `tio_handler_set_cb_json_parser_resource()` | nothing: delete the call |
| `tio_updater_set_json_parser_resource()`, `tio_updater_set_cb_json_parser_resource()` | nothing: delete the call |

`JKII_ERR_TOKENS_SHORTAGE` and `JKII_ERR_ALLOCATION` went with them, and the
memory the tokens took can go too. `jkii.h` no longer includes `jsmn.h`, which
isn't installed, so code that used jsmn's names through it, such as
`jsmntok_t` or `jsmn_parse()`, has to have jsmn of its own.

**`kii_set_mqtt_to_sock_send()` and `tio_handler_set_mqtt_to_sock_send()` are
gone.** Delete the call. They stored a value nothing read, in 1.x as well, so
this changes nothing at run time. An MQTT socket still needs a send timeout,
which is the application's to set in its socket, as the samples do.

### Values whose type changed

These still compile. Check anything that depends on their width: format
strings, pointers to them, and narrower variables they are stored in.

| Where | What | 1.x | 3.0 |
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
  `tio_handler_t` and `tio_updater_t` changed. Code built against 1.x's headers
  can't use 3.0's libraries.
- The libraries are `libebisu_khc`, `libebisu_jkii`, `libebisu_kii` and
  `libebisu_tio`, all `.so.3`, with `libebisu_core` below them, where 1.x's were
  `libkhc`, `libjkii`, `libkii` and `libtio`, and `libkhc` and `libjkii` had no
  version in their soname. Linking them is under Building.
- The shared libraries export only what the public headers declare. Code that
  called anything else in them, such as `_extract_content_length()` or
  `_get_anonymous_token()`, no longer links.
- `kii_t`, `tio_handler_t` and `tio_updater_t` are about 700 bytes larger on
  x86-64, and `kii_author_t` goes from 256 to 384 bytes with `refresh_token`.
  Check any static memory or task stack that holds one.
- A device that saved a `kii_author_t` as bytes can read 1.x's into a zeroed
  3.0 one: `author_id` and `access_token` keep their offsets, and `refresh_token`
  comes after them.

### Building

Without CMake, as a vendor's build usually does:

- Add `core/src/*.c`, `khc/src/khc_http_date.c`, `kii/kii_clock.c` and
  `kii/kii_session.c` to the sources, and `core/include` to the include path.
  A port, for a system ebisu has one for, is under `platform/`.
- `jkii/libs/jsmn` is gone. Leave `jkii/libs/jsmn/jsmn.c` out, and put
  `jkii/libs` on the include path of jkii's sources, whose `jkii_core_json.c`
  compiles coreJSON under names of jkii's own. Don't build
  `jkii/libs/coreJSON/core_json.c` itself as well, which would be a second copy
  that nothing calls.
- The sources now include a generated `ebisu_version.h`. Write it with
  `version/ebisu_version.sh <output>` and put it on the include path, as the
  Makefiles of the embedded samples do.
- `tio/wiced-sample` is gone. It was only a sample, and nothing in the libraries
  depends on it.

With CMake:

- CMake 3.15 is the minimum, up from 3.12. The presets need 3.21.
- There is one project, at the root of the repository. The `CMakeLists.txt` of
  `jkii`, `khc`, `kii` and `tio` no longer build on their own: configure from
  the root, or `add_subdirectory()` it.
- The Makefiles at the root, in each component, in `tio/linux-sample` and in the
  tests are gone. See Building in the [README](./README.md#building). The cc3200
  and gt202 samples keep theirs.
- `tio/arm-toolchain.cmake`, for armv5t with soft floating point, and
  `tio/README.ARM.mkd` are gone. `cmake/arm-linux-gnueabihf.cmake` cross-compiles
  for armv7-a with hard floating point; see Building in the
  [README](./README.md#building) for another ARM target.
- `KHC_TRACE` is off by default, where 1.x had it on. Pass `-DKHC_TRACE=ON` to
  get the trace back. `TIO_TRACE`, which no option set in 1.x, is an option now,
  and off too.

Linking:

- Link `-lebisu_tio` where 1.x linked `-ltio`, and likewise for the others,
  with no aliases for the old names.
- Each library holds its own code alone, and links the ones below it. In 1.x,
  `libkii` held khc's and jkii's code too, and `libtio` all of it, so `-ltio` was
  enough. Against the static libraries, or calling any of them directly, link
  them all, in this order: `-lebisu_tio -lebisu_kii -lebisu_khc -lebisu_jkii
  -lebisu_core`.
- The CMake package's targets, `ebisu::tio` and the others, and the pkg-config
  files, `ebisu-tio` and the others, keep their names, and bring in what each
  library links. A build using either changes nothing.

### Behaviour changes

All of this compiles unchanged. Most of it is ebisu reporting what it used to
hide, so an application can meet errors it has never had to handle.

Starting tasks:

- `tio_updater_start()` returns `TIO_ERR_FAIL` until `tio_updater_set_interval()`
  has set an interval above 0. 1.x uploaded as fast as it could instead.
- `kii_start_push_task()` and `tio_handler_start()` refuse a keep-alive above
  65535, with `KII_ERR_FAIL` and `TIO_ERR_FAIL`. 1.x sent the broker the value
  cut down to 16 bits.
- The result of the system's `task_create` counts, so it has to return
  `EBISU_TASKC_OK` when it succeeds. Anything else makes `kii_start_push_task()`
  return the new `KII_ERR_CREATE_TASK` and `tio_handler_start()` return
  `TIO_ERR_CREATE_TASK`, where 1.x ignored it and reported success. No system,
  or one without a `task_create`, gives the same codes, and
  `TIO_ERR_CREATE_TASK` from `tio_updater_start()`.

- `kii_start_push_task()`, `tio_handler_start()` and `tio_updater_start()` return
  `KII_ERR_FAIL` or `TIO_ERR_FAIL` if something the task uses isn't set: the
  callback they are given, or for tio the state reader, the system's
  `delay_ms`, a socket or one of its operations, or a buffer. 1.x started a
  task that crashed on reaching it.
- They return the same while the task of an earlier start is still running,
  where 1.x started a second one on the same sockets and buffers. It stops
  running just before its exit callback, which can start it again. A task ended
  any other way, such as by killing its thread, needs `kii_init()`,
  `tio_handler_init()` or `tio_updater_init()` before starting again.

The push task:

- It closes its MQTT connection before calling the exit callback. An exit
  callback that closes it too now closes it twice, so it should only free what
  it owns. On a connection the broker accepted, it sends DISCONNECT first, as
  1.x never did.
- A network failure while installing push or fetching the endpoint is tried
  again, where 1.x ended the task. An application no longer sees the exit
  callback for these, and needn't restart the handler.
- It speaks MQTT 3.1.1, where 1.x spoke 3.1. Kii Cloud's broker takes both. For
  a broker with nothing later than 3.1, which refuses CONNECT with return code
  1, set 3.1 with `kii_set_mqtt_version()` or `tio_handler_set_mqtt_version()`.
- It reconnects when a PINGREQ is still unanswered by the time the next is due.
  1.x only noticed a dead connection when sending failed, so a device on a link
  too slow to answer within the keep-alive interval now reconnects.
- Failed attempts back off, from about a second up to five minutes, where 1.x
  tried every second or two. A device can take up to five minutes to come back
  after a long outage. Reaching a subscribed connection starts the waits again.
  After 8 failures in a row the task asks for the endpoint again.
  `kii_set_mqtt_retry_wait_max()` and `tio_handler_set_mqtt_retry_wait_max()`
  set another longest wait.

Commands in tio:

- Results the server refuses are reported as `TIO_ERR_RESP_STATUS`, through the
  error callback and from `tio_handler_handle_command()`. 1.x reported nothing.
  The action has already run by then.
- A command with an empty alias or action object, or with an action value jkii
  can't parse, is `TIO_ERR_PARSE_JSON` and doesn't reach the action callback.

kii:

- A buffer set with `kii_set_stream_buff()` or `kii_set_resp_header_buff()` is
  used by every request from then on, until it is set again, so it has to stay
  valid, and be used for nothing else, while the `kii_t` is in use. 1.x used it
  only when it was set between `kii_api_call_start()` and `kii_api_call_run()`,
  gave every other request buffers of khc's own, and said it could be freed
  once the setter returned. Set `NULL` before freeing it. The handler's and the
  updater's setters pass theirs on, so a `tio_handler_t` and a `tio_updater_t`
  can no longer share one: their tasks would use it at once.
- After `kii_auth_thing()`, `kii_register_thing()`, `kii_ti_onboard()` and
  `kii_get_mqtt_endpoint()` succeed, the buffers set by `kii_set_buff()` and
  `kii_set_stream_buff()` are cleared, and `kii_get_resp_body_length()` is 0:
  their requests and responses held a password, tokens or MQTT credentials. 1.x
  left the response there. One that refused the request stays, as it says why.
- A request whose body kii holds whole in its buffer, every request but a state
  or an object body streamed through a callback of the application's, is sent
  with `Content-Length` rather than `Transfer-Encoding: chunked`. A streamed body
  is still chunked.
- `kii_set_site()` takes the host name of the application's site. 1.x also
  took `"US"`, `"SG"`, `"EU"` and `"CN3"` for hosts it knew, which are now taken
  as host names themselves: pass the host instead. `"JP"` still gives
  `"api-jp.kii.com"`, the public site. `tio_handler_set_app()` and
  `tio_updater_set_app()`, which pass theirs on, took them too.
- With a system set by `kii_set_system()`, `tio_handler_set_system()` or
  `tio_updater_set_system()`, a request whose socket returns `EBISU_SOCK_AGAIN`
  waits 10 ms with the system's `delay_ms` before calling it again, where 1.x
  called it again at once.
- `kii_get_mqtt_endpoint()` fails on a port outside 0 to 65535, or a TTL that is
  negative or above `UINT32_MAX`.
- `kii_ti_put_firmware_version()` escapes the version, as onboarding always has.
  A caller that escaped it first should stop.
- `kii_api_call_append_header()` returns `KII_ERR_TOO_LARGE_DATA` for a header
  longer than the buffer set by `kii_set_buff()`. 1.x took any length, on the
  stack of the task calling it.
- `kii_api_call_start()` returns `KII_ERR_TOO_LARGE_DATA`, sending nothing, for
  a path of 256 bytes or more or a method of 16 or more, and so do
  `kii_execute_server_code()` and `kii_execute_server_code_version()` for an
  endpoint whose path, with the application's ID and the version, comes to 256
  bytes or more. 1.x sent the request all the same, to the wrong path.
- A response too large for the buffer set by `kii_set_buff()` is
  `KII_ERR_TOO_LARGE_DATA`, and `TIO_ERR_TOO_LARGE_DATA` in tio. In 1.x it was
  `KII_ERR_WRITE_CALLBACK` and `TIO_ERR_WRITE_CALLBACK`. `KII_ERR_WRITE_CALLBACK`
  stays for a write callback given to `kii_download_object_body()`, and
  `TIO_ERR_WRITE_CALLBACK` stays defined but isn't returned.

khc, which kii and tio report in their own codes:

- A connection that closes before the response ends is `KHC_ERR_SOCK_RECV`. In
  1.x, `khc_perform()` didn't return if that happened during the headers, and
  returned `KHC_ERR_OK` short of a `Content-Length` and `KHC_ERR_FAIL` in a
  chunked body.
- An invalid `Content-Length`, or a `Transfer-Encoding` whose final coding isn't
  `chunked`, is `KHC_ERR_HEADER_CALLBACK`, before any of the body is written.
- A response ends at its `Content-Length`, or after the trailers of a chunked
  body, and `khc_perform()` then closes the connection. In 1.x it read on until
  the server closed it, so a server that was slow to close turned a whole
  response into `KHC_ERR_SOCK_RECV`, and whatever came after the body reached
  the write callback. A response to `HEAD`, a `204` and a `304` end with their
  headers. A body with neither a length nor chunks still ends with the
  connection.
- `Content-Length` headers that disagree are `KHC_ERR_HEADER_CALLBACK`.
- An invalid chunk size, or a first line that isn't an HTTP status line, is
  `KHC_ERR_FAIL`.
- A socket that fails to close no longer changes a request's result: a response
  read whole is `KHC_ERR_OK`, and a request that failed keeps its failure. In
  1.x it was `KHC_ERR_SOCK_CLOSE`, which kii and tio reported as a failure to
  connect. That code, `KII_ERR_SOCK_CLOSE` and `TIO_ERR_SOCK_CLOSE` stay defined
  but aren't returned, and `khc_get_close_failed()` says whether it happened.
- A `Content-Length` or `Transfer-Encoding` header too long for the buffer set
  by `khc_set_resp_header_buff()` is `KHC_ERR_TOO_LARGE_DATA`, and
  `KII_ERR_TOO_LARGE_DATA` and `TIO_ERR_TOO_LARGE_DATA` above it. In 1.x it was
  `KHC_ERR_HEADER_CALLBACK`. Any other header too long is still skipped, and
  `khc_get_resp_header_skipped()` says whether one was.
- `khc_perform()` returns `KHC_ERR_FAIL`, without connecting, if a socket
  callback, the write callback or the header callback isn't set. 1.x crashed on
  reaching it.
- GET, HEAD and DELETE send no body, and don't call the read callback.
- Each chunk of a request body is sent in one send, with its size line and CRLF,
  from the stream buffer. 1.x sent the three separately. The read callback is
  offered the stream buffer less room for those, a few bytes, rather than all
  of it, and a stream buffer with no room left for data, of 5 bytes or fewer,
  is `KHC_ERR_TOO_LARGE_DATA`. A read callback that returns more than it was
  asked for is `KHC_ERR_READ_CALLBACK`, a new code, and `KII_ERR_FAIL` in kii.
- Request lines carry the path, as in `POST /path HTTP/1.1`, with the host in the
  `Host` header, which is what a server expects. 1.x sent the whole URL, as in
  `POST https://host/path HTTP/1.1`. A forward proxy that requests go through in
  plain HTTP needs the URL: ask for it with `khc_enable_absolute_target()`,
  `kii_enable_absolute_target()`, `tio_handler_enable_absolute_target()` or
  `tio_updater_enable_absolute_target()`.
- `khc_slist_free_all()` and `khc_slist_free_all_using_cb_free()` clear each
  node's string before freeing it, as a request header may carry a token. A
  free callback finds it all zeros.

jkii:

- An integer's type follows its value: `int32_t`, then `int64_t`, then overflow
  or underflow, whatever its length. 1.x went by the number of characters, so
  `2147483648` could be an overflow, and very long integers were doubles. A
  double field given one now reports a type mismatch.
- Integers with leading zeros are invalid, path indices such as `/[010]`
  included. 1.x read them as octal. `0` and `-0` are still numbers.
- Numbers without digits, such as `-`, `1e` and `1e+`, are invalid.
- Only `null`, `true` and `false` in full are those values. 1.x took a prefix of
  one, such as `n` or `t`, for it.
- Empty or whitespace-only input is `JKII_ERR_INVALID_INPUT`.
- The whole string is checked to be JSON, as ECMA-404 has it, before anything
  is looked up, and anything else is `JKII_ERR_INVALID_INPUT`: an unquoted or
  non-string key, as in `{x:1}`; a missing or stray comma or colon, as in
  `{"a":1,}` or `{"a"{"b":1}}`; a key without a value, as in `{"a"}`; a control
  character or a bad escape in a string; text after the root object or array.
  So is nesting objects and arrays deeper than 32 levels. 1.x, with jsmn's
  lenient grammar, took them all, `{x:1}` for an object whose key no lookup
  found and the key before a missing colon for the parent of what followed.
- Fields looked up by name are found only in an object. In an array, 1.x took
  the elements for pairs of a name and a value. Fields looked up by path still
  reach an array's elements.
- A path lookup doesn't find a key that isn't a string, or follow a path that
  isn't valid.
- `jkii_escape_str()` can write six bytes for each one it reads, up from two. It
  returns -1 if the buffer is too small, so size one for it at
  `strlen(str) * 6 + 1`.

### If you start using sessions

`kii_session_t` is new and optional. Without one, tokens are handled as in 1.x.
With one:

- A `TIO_CB_SIZE` callback has to restart its `TIO_CB_READ` reader whenever it is
  called, as the updater uploads again after a token is renewed.
- A renewal keeps a copy of `khc` on the stack of the task making the request,
  about 700 bytes on x86-64. Allow for it in that task's stack.
