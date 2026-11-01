# kii {#kii_readme}

`kii` gives a device access to Kii Cloud's REST API and Thing-IF:

- authenticating the thing, by onboarding or logging in, and keeping its token
  valid through a session;
- objects in buckets, with bodies of any size;
- push messages over MQTT, from buckets, topics or straight to the thing;
- Thing-IF states and firmware versions;
- server code, and any other REST API call.

[tio](https://github.com/KiiPlatform/ebisu/blob/master/tio/README.md) is built on `kii`
and covers what most devices need: receiving commands and uploading states.
Use `kii` directly for the rest.

`kii` does no networking of its own. It sends HTTP requests with
[khc](https://github.com/KiiPlatform/ebisu/blob/master/khc/README.md), through socket
callbacks the application supplies, and runs MQTT in a task the application
creates.

## Set up a `kii_t`

```c
    kii_t kii;
    char buff[4096];
    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    kii_init(&kii);
    kii_set_site(&kii, "JP");
    kii_set_app_id(&kii, "your-app-id");

    kii_set_buff(&kii, buff, sizeof(buff));
    kii_set_json_parser_resource(&kii, &resource);

    kii_set_cb_http_sock_connect(&kii, sock_cb_connect, &http_sock_ctx);
    kii_set_cb_http_sock_send(&kii, sock_cb_send, &http_sock_ctx);
    kii_set_cb_http_sock_recv(&kii, sock_cb_recv, &http_sock_ctx);
    kii_set_cb_http_sock_close(&kii, sock_cb_close, &http_sock_ctx);
```

- **Site**: one of `"JP"`, `"US"`, `"EU"`, `"SG"`, `"CN"` and `"CN3"`, or the
  host name of your application's site.
- **Buffer**: required. Requests are built in it and responses read into it,
  so it must hold the largest of them: 4096 bytes are typically enough. Object
  bodies and Thing-IF states stream through callbacks instead, whatever their
  size. With a session, leave room after a request's body for the session to
  renew the token and send the request again; see [Sessions](#kii_session).
- **JSON parser resource**: tokens `jkii` parses responses with. 256 are
  enough for most responses. `kii_set_cb_json_parser_resource()` allocates
  exactly as many as each response needs instead.
- **Socket callbacks**: see
  [khc's socket callbacks](https://github.com/KiiPlatform/ebisu/blob/master/khc/README.md).
  They are asked to connect to port 443, for TLS, or to port 80 when
  `kii_enable_insecure_http()` is set.

`kii` also uses the stream and response header buffers that `khc` does. Set
them with `kii_set_stream_buff()` and `kii_set_resp_header_buff()`, or `kii`
allocates them with `malloc()` for each request. Request headers are allocated
with `malloc()` too, unless `kii_set_cb_slist_resource()` gives allocators of
your own.

A `kii_t` makes one request at a time, and APIs block until the response has
been read. Tasks making requests at the same time each need a `kii_t` of their
own, with buffers of their own, attached to the same [session](#kii_session).

## Authenticate the thing

Most APIs are authorized with the thing's access token. Get one by:

- **Onboarding** through Thing-IF, which registers the thing the first time and
  returns its ID and a token every time. Where MQTT is enabled, it also returns
  the MQTT endpoint the push task connects with.

  ```c
    kii_code_t res = kii_ti_onboard(&kii, vendor_thing_id, password,
            "your-thing-type", "1.0.0", NULL, NULL);
  ```

  The thing type, firmware version, layout position and properties are set the
  first time only, and can be `NULL`. Properties are a JSON object.
- **Logging in** a thing already registered, with `kii_auth_thing()`, or
  **registering** one, with `kii_register_thing()`.

The thing's ID and token are then kept in the `kii_t`, or in its session, and
`kii_get_author()` copies them:

```c
    kii_author_t author;
    kii_get_author(&kii, &author);
    printf("thing ID: %s\n", author.author_id);
```

<a name="kii_session"></a>
## Sessions

Access tokens expire. A `kii_session_t` holds the thing's tokens for every
`kii_t` acting as the thing, and renews the access token the way it was
obtained:

- **From onboarding**: by onboarding again through Thing-IF, when the server
  refuses the token. Onboarding issues no refresh token and no lifetime, and
  onboarding again leaves earlier tokens valid.
- **From a login or registration**: with the refresh token, or by logging in
  again when there is none or the server refuses it. This happens shortly
  before the token expires, and whenever the server refuses it.

The server refuses a token with `403` and the errorCode `WRONG_TOKEN`. The
refused request is then sent again with the new token, provided its body was
built in the buffer set by `kii_set_buff()`, and the buffer has room after the
body for the renewal: about 640 bytes, or 1150 to onboard again. Bodies
streamed through callbacks, such as object bodies and Thing-IF states, can't
be read again: the API reports the `403`, the token already renewed, and
calling it again uses the new token.

```c
    kii_session_t session;
    kii_session_init(&session);
    kii_session_set_cb_lock(&session, lock_cb, unlock_cb, &session_mutex);
    kii_session_set_cb_monotonic_ms(&session, monotonic_ms_cb, NULL);
    kii_session_set_password(&session, vendor_thing_id, password);
    kii_session_set_cb_author_updated(&session, author_updated_cb, NULL);

    if (kii_set_session(&kii, &session) != KII_ERR_OK) {
        /* No lock or clock set. */
    }
    kii_code_t res = kii_ti_onboard(&kii, vendor_thing_id, password,
            NULL, NULL, NULL, NULL);

    kii_author_t author;
    kii_session_get_author(&session, &author);
    printf("thing ID: %s\n", author.author_id);
```

Attach the session to every `kii_t` acting as the thing before onboarding or
logging in. The callbacks:

- **Lock**: required. Keeps one task from renewing the token while another
  does, held for the length of the renewal request. It need not be recursive.
  On a platform where no other task using the session can run meanwhile, it
  can do nothing.
- **Monotonic clock**: required. Milliseconds since any fixed origin, never
  going back, and not jumping when the wall clock is set. A 32-bit tick counter
  has to be extended to 64 bits. It is called only with the lock held.
- **Credentials**: the vendor thing ID and password to onboard or log in again
  with. Without them, a token onboarding obtained is never renewed. The strings
  are not copied. Once the server refuses them, they are not tried again for 5
  minutes, then twice as long after each further refusal, up to an hour: where
  the application limits login attempts, trying a wrong password on every
  refused request would soon lock the thing out. Setting them again ends the
  wait.
- **Author updated**: called with the lock held whenever the tokens change.
  A refresh invalidates the tokens it replaces, so a device that keeps its
  tokens across a restart must save the author here before returning. It must
  not call `kii` APIs.

To restore a saved author, give it to the session with
`kii_session_set_author()`. If onboarding obtained it,
also call `kii_session_set_renew_mode()` with `KII_RENEW_BY_ONBOARDING`, or the
session renews it through Kii Cloud's token endpoint.

The [tio README](https://github.com/KiiPlatform/ebisu/blob/master/tio/README.md) has lock
and clock implementations for `pthread`.

## Objects

Objects live in buckets, which belong to the application, a user, a group or a
thing:

```c
    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.scope_id = author.author_id;
    bucket.bucket_name = "readings";

    kii_object_id_t object_id;
    kii_code_t res = kii_post_object(&kii, &bucket,
            "{\"temperature\":21.5}", NULL, &object_id);

    res = kii_get_object(&kii, &bucket, object_id.id);
    if (res == KII_ERR_OK) {
        printf("%.*s\n", (int)kii_get_resp_body_length(&kii), buff);
    }
```

- `kii_put_object()` replaces an object and `kii_patch_object()` updates some
  of its fields. Both take an optional ETag, from `kii_get_etag()` after the
  object was read, so that the update fails if the object has changed since.
- `kii_delete_object()` deletes one.
- `kii_upload_object_body()` and `kii_download_object_body()` stream an
  object's body, of any size and content type, through a read or write
  callback.

## Push

The push task keeps an MQTT connection to Kii Cloud's broker and hands every
message it receives to a callback. The application creates the task, and
supplies the MQTT socket callbacks and a buffer for messages:

```c
    char mqtt_buff[2048];
    kii_set_mqtt_buff(&kii, mqtt_buff, sizeof(mqtt_buff));

    kii_set_cb_mqtt_sock_connect(&kii, sock_cb_connect, &mqtt_sock_ctx);
    kii_set_cb_mqtt_sock_send(&kii, sock_cb_send, &mqtt_sock_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, sock_cb_recv, &mqtt_sock_ctx);
    kii_set_cb_mqtt_sock_recv_timed_out(&kii, sock_cb_recv_timed_out, &mqtt_sock_ctx);
    kii_set_cb_mqtt_sock_close(&kii, sock_cb_close, &mqtt_sock_ctx);
    kii_set_mqtt_to_sock_recv(&kii, 15);
    kii_set_mqtt_to_sock_send(&kii, 15);

    kii_set_cb_task_create(&kii, task_create_cb, NULL);
    kii_set_cb_delay_ms(&kii, delay_ms_cb, NULL);
    kii_set_cb_task_continue(&kii, task_continue_cb, NULL);
    kii_set_cb_task_exit(&kii, task_exit_cb, NULL);

    kii_code_t res = kii_start_push_task(&kii, 300, push_received_cb, NULL);
```

```c
void push_received_cb(const char* message, size_t message_length, void* userdata)
{
    /* The message as Kii Cloud sent it, in JSON. Not null-terminated. */
    printf("%.*s\n", (int)message_length, message);
}
```

- **MQTT sockets**: blocking, with receive and send timeouts the same as those
  given to `kii_set_mqtt_to_sock_recv()` and `kii_set_mqtt_to_sock_send()`.
  When the receive timeout runs out, the recv callback returns
  `KHC_SOCK_FAIL`. The optional `kii_set_cb_mqtt_sock_recv_timed_out()`
  callback tells such a timeout from a lost connection, after which the task
  connects again.
- **Keep-alive**: the second argument of `kii_start_push_task()`, in seconds.
  It must be longer than the socket timeouts; a few minutes is recommended. 0
  turns it off, and a lost connection may then go unnoticed.
- **Task callbacks**: creating the task and delaying it are required.
  `kii_set_cb_task_continue()` lets the application stop the task, and
  `kii_set_cb_task_exit()` is called as it exits, to free what it used. Both
  receive a `kii_mqtt_task_info*` saying what the task was doing and why it
  stopped. The [tio README](https://github.com/KiiPlatform/ebisu/blob/master/tio/README.md)
  has implementations for `pthread`.

After onboarding, the task connects with the MQTT endpoint onboarding returned.
Otherwise it installs push and gets an endpoint from Kii Cloud first. A request
that fails on the network, or with a `5xx` or `429` response, is tried again, so
a device can start the task before it has a network. Any other response ends
the task, with the reason in `kii_mqtt_task_info`. The task uses the `kii_t` it
was started with until it exits: make requests from other tasks with another
`kii_t`.

While it can't reach the cloud or the broker, the task waits longer between
attempts: from about a second to at most five minutes, doubling each time, and
for a share of that which depends on the thing, so that devices that lost their
connection together don't all come back together. The wait goes back to the
start once a connection is subscribed. The continue callback is still called
every second, so the application can stop the task during a wait. A broker it
can't reach after 8 attempts may have moved, so it then asks for the endpoint
again.

What reaches the thing:

- **Buckets and topics** it subscribes to, with `kii_subscribe_bucket()` and
  `kii_subscribe_topic()`. `kii_put_topic()` creates a topic, and
  `kii_delete_topic()` deletes one.
- **Messages sent to a topic** with `kii_send_push_message()`, and **straight
  to a thing** with `kii_send_push_message_to_thing()`:

  ```c
    kii_topic_t topic;
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = author.author_id;
    topic.topic_name = "alerts";

    res = kii_send_push_message(&kii, &topic,
            "{\"data\":{\"level\":\"high\"},\"mqtt\":{\"enabled\":true}}");
  ```

## Thing-IF

- `kii_ti_put_state()` and `kii_ti_patch_state()` upload the thing's state, and
  `kii_ti_put_bulk_states()` and `kii_ti_patch_bulk_states()` several at once,
  read through a callback until it returns 0. `tio_updater_t` does this
  periodically.
- `kii_ti_put_firmware_version()` and `kii_ti_get_firmware_version()` set and
  read the thing's firmware version.

Commands arrive as push messages. `tio_handler_t` parses them, runs their
actions and sends the results back.

## Server code

```c
    res = kii_execute_server_code(&kii, "your-endpoint", "{\"param\":1}");
```

The result is in the buffer, `kii_get_resp_body_length()` bytes long.
`kii_execute_server_code_version()` runs a given version of the code.

## Other REST API calls

Any other call can be built with `kii_api_call_*()`. The path is the whole
path, application ID included:

```c
    kii_api_call_start(&kii, "POST",
            "/api/apps/your-app-id/buckets/readings/objects",
            "application/json", KII_TRUE);
    kii_api_call_append_body(&kii, "{\"temperature\":", 15);
    kii_api_call_append_body(&kii, "21.5}", 5);
    res = kii_api_call_run(&kii);
    if (res == KII_ERR_OK) {
        printf("status: %u\n", (unsigned)kii_get_resp_status(&kii));
    }
```

`KII_TRUE` authorizes the call with the thing's token. Headers can be added
with `kii_api_call_append_header()`. `kii_api_call_run()` returns `KII_ERR_OK`
whatever the status, which `kii_get_resp_status()` gives. The response body is
in the buffer, `kii_get_resp_body_length()` bytes long.

## Results

APIs return `KII_ERR_OK` on success. `KII_ERR_RESP_STATUS` means the server
answered with an error status, which `kii_get_resp_status()` gives, the
response body being in the buffer. `KII_ERR_SOCK_*` come from the socket
callbacks, and `KII_ERR_TOO_LARGE_DATA` from a request or response that doesn't
fit in the buffer.

`kii_get_resp_date()` gives the server's time, from the `Date` header of the
call's response, for a device without a real-time clock: khc's
`khc_get_resp_date()`, which the [khc README](../khc/README.md#results)
describes. It is as trustworthy as the connection it came through, so verify
the server's certificate in the socket callbacks.

## The time

A device without a real-time clock needs the time to check the validity period
of the server's certificate in its socket callbacks. A `kii_clock_t` keeps it.
Until a server states its time, it has a floor: a time it is known not to be
earlier than. From the first response on, it has the time the response states,
counted on with the monotonic clock.

```c
    kii_clock_t clock;
    kii_clock_init(&clock, monotonic_ms_cb_impl, NULL);
    // Needed when kii_t of more than one task share the clock. Not the
    // session's lock: the socket callbacks may use the clock while kii holds
    // that one.
    kii_clock_set_cb_lock(&clock, lock_cb_impl, unlock_cb_impl, &clock_mutex);
    // A time the device saved before, if any. Without one, the floor is
    // shortly before the library's source was built.
    kii_clock_set_floor(&clock, saved_time);
    kii_set_clock(&kii, &clock);
```

The socket callbacks then check the server's certificate against it:

- `kii_clock_check_validity()` checks a validity period. With the floor only, a
  certificate that ended before it has expired, but one that starts later
  cannot be told from one that is valid now, so only the end is checked.
  `khc_seconds_since_epoch()` converts the dates a TLS stack gives.
- `kii_clock_get_time()` gives the time, and whether it is the time or only the
  floor, for a TLS stack that checks validity itself: give it the time, and
  while it is only the floor, ignore a certificate not being valid yet.
- `kii_clock_forget_time()`, when the server's certificate is refused as not
  valid yet while the clock had the time. That suggests the time fell behind,
  the monotonic clock having stopped while the device slept and the server
  having renewed its certificate meanwhile. Forgotten, the time becomes a lower
  bound, the next connection goes through, and its response gives the clock the
  time again. Without it, the connection that would bring the time is refused,
  again and again.

The chain and the host name are checked in full either way: the clock only
decides the validity period.

The socket callbacks of the [Linux](../tio/linux-sample/sock_cb_linux.c) and
[Win32](../tio/win32-sample/sock_cb_win32.c) samples give their TLS stack the
time, and those of the [ESP32](../tio/esp32-sample/main/sock_cb_esp32.c) and
[FreeRTOS](../tio/freertos-sample/sock_cb_freertos.c) samples check the
validity periods themselves, mbedTLS not checking them: the ESP32 one after the
handshake, the FreeRTOS one during it, in mbedTLS's verify callback.

A device that runs for a long time has the time again with every HTTP response.
One that makes none for a while, only MQTT ones, has its time turn into a lower
bound 24 hours after the last (`kii_clock_set_exact_for()` changes that), and a
monotonic clock that goes back turns it into the floor: a monotonic clock
drifts, and one cannot count on from a time when it goes back. A stale clock
therefore never refuses a certificate as not yet valid, which could keep the
device from connecting at all. The monotonic clock has to keep counting while
the device sleeps, or the time falls behind while it still counts as exact.
Saving the time now and then, and setting it with `kii_clock_set_floor()` after
a restart, keeps the floor close to the time.

## Building

`kii` is built as part of ebisu, as described in the
[top-level README](https://github.com/KiiPlatform/ebisu/blob/master/README.md#building).
`libkii` includes `khc` and `jkii`.
