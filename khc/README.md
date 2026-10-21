# khc {#khc_readme}

`khc` is a compact HTTP/1.1 client for resource-limited devices. `kii` and
`tio` send their requests with it, so most applications use those and never
call `khc` directly.

`khc` does no networking of its own: the application supplies socket
callbacks, which also do TLS. Request and response bodies stream through
callbacks and a buffer of fixed size, so neither has to fit in memory.

## Supported features

- **Requests**: any method, sent to the host's port 443, or 80 when
  `khc_enable_insecure()` is set. The request line carries the whole URL, as in
  `GET https://api.kii.com/path HTTP/1.1`, followed by a `HOST` header and the
  application's headers.
- **Request bodies**: sent with `Transfer-Encoding: chunked`, one chunk for
  each call to the read callback, until it returns 0. Without a read callback,
  the request is sent with `Content-Length: 0`. `GET`, `HEAD` and `DELETE`
  requests are sent without a body.
- **Responses**: the status line and each header are passed to the header
  callback, and the body to the write callback. After an interim `1xx`
  response, which goes through the header callback too, `khc` reads the final
  one.
  - A body sent with the chunked transfer coding is decoded; chunk extensions
    and trailers are ignored.
  - Any other body is read until the server closes the connection, which it
    does after the body, as every request is sent with `Connection: Close`.
- **Non-blocking sockets**: a socket callback can return `KHC_SOCK_AGAIN`, and
  `khc` calls it again with the same arguments.

## Limitations

- **One request per connection**: no keep-alive and no pipelining.
  `khc_perform()` connects, sends the request, reads the response and closes
  the connection, blocking until it is done.
- **Nothing beyond HTTP framing**: redirects are not followed, and `3xx`
  responses reach the caller like any other. There's no support for
  compression, cookies or proxies. Authorization and content types are headers
  the application sets.
- **Transfer codings**: a response whose last transfer coding is not
  `chunked` is refused with `KHC_ERR_HEADER_CALLBACK`, as is one with a
  malformed `Content-Length`.
- **Fixed-size request fields**: a host longer than 127 bytes, a path longer
  than 255 and a method longer than 15 are refused with
  `KHC_ERR_TOO_LARGE_DATA`.
- **Long response headers**: a header that doesn't fit in the response header
  buffer is skipped without reaching the header callback. If it is
  `Content-Length` or `Transfer-Encoding`, which `khc` needs, the request fails
  with `KHC_ERR_HEADER_CALLBACK`.

## Memory management

A `khc` instance holds its request line fields and state, and otherwise uses
two buffers:

- **Stream buffer**: holds each chunk of the request body and of the response
  body as it is read. It must hold at least a chunk size line, and a larger one
  means fewer callbacks. Set it with `khc_set_stream_buff()`. Otherwise
  `khc_perform()` allocates 1024 bytes with `malloc()` and frees them before
  returning.
- **Response header buffer**: holds one response header at a time, and must
  fit the status line, `Content-Length` and `Transfer-Encoding`. 256 bytes are
  enough. Set it with `khc_set_resp_header_buff()`. Otherwise `khc_perform()`
  allocates 256 bytes with `malloc()` and frees them before returning.

Request headers are a `khc_slist`, which belongs to the application and must
stay valid until `khc_perform()` returns. `khc_slist_append()` allocates each
node with `malloc()`, and `khc_slist_free_all()` frees the list.
`khc_slist_append_using_cb_alloc()` and `khc_slist_free_all_using_cb_free()`
take an allocator of your own instead, such as one handing out nodes from a
static pool.

With both buffers set and the headers allocated by your own allocator, `khc`
never calls `malloc()`.

## Making a request

The header and write callbacks are required. Each returns the number of bytes
it handled; anything else aborts the request with `KHC_ERR_HEADER_CALLBACK` or
`KHC_ERR_WRITE_CALLBACK`.

```c
static size_t on_header(char* buffer, size_t size, void* userdata)
{
    /* The status line first, then one header per call, without CRLF. */
    printf("%.*s\n", (int)size, buffer);
    return size;
}

static size_t on_write(char* buffer, size_t size, void* userdata)
{
    /* Part of the body. Not null-terminated. */
    fwrite(buffer, 1, size, stdout);
    return size;
}
```

A `GET` request:

```c
    khc http;
    char stream_buff[1024];
    char header_buff[256];

    khc_init(&http);
    khc_set_host(&http, "example.com");
    khc_set_path(&http, "/index.html");
    khc_set_method(&http, "GET");

    const char accept[] = "Accept: application/json";
    khc_slist* headers = khc_slist_append(NULL, accept, strlen(accept));
    khc_set_req_headers(&http, headers);

    khc_set_stream_buff(&http, stream_buff, sizeof(stream_buff));
    khc_set_resp_header_buff(&http, header_buff, sizeof(header_buff));

    khc_set_cb_sock_connect(&http, sock_cb_connect, &sock_ctx);
    khc_set_cb_sock_send(&http, sock_cb_send, &sock_ctx);
    khc_set_cb_sock_recv(&http, sock_cb_recv, &sock_ctx);
    khc_set_cb_sock_close(&http, sock_cb_close, &sock_ctx);
    khc_set_cb_header(&http, on_header, NULL);
    khc_set_cb_write(&http, on_write, NULL);

    khc_code res = khc_perform(&http);
    if (res == KHC_ERR_OK) {
        printf("status: %u\n", (unsigned)khc_get_status_code(&http));
    }
    khc_slist_free_all(headers);
```

To send a body, use a method that has one and set a read callback. It is
called until it returns 0:

```c
typedef struct {
    const char* data;
    size_t length;
    size_t sent;
} body_t;

static size_t on_read(char* buffer, size_t size, void* userdata)
{
    body_t* body = (body_t*)userdata;
    size_t remaining = body->length - body->sent;
    size_t n = remaining < size ? remaining : size;
    memcpy(buffer, body->data + body->sent, n);
    body->sent += n;
    return n;
}
```

```c
    body_t body = { json, strlen(json), 0 };
    khc_set_method(&http, "POST");
    khc_set_cb_read(&http, on_read, &body);
```

To make another request with the same callbacks, call `khc_reset_except_cb()`,
then set the request and the buffers again. To send the same request again,
call `khc_reset_except_request()`, which keeps the buffers too. The read
callback must then start the body over.

## Socket callbacks

Declared in `khc_socket_callback.h`. Each returns `KHC_SOCK_OK`,
`KHC_SOCK_FAIL`, or `KHC_SOCK_AGAIN` to be called again with the same
arguments.

- **Connect** (`sock_ctx`, `host`, `port`): opens the connection, doing the TLS
  handshake unless the connection is insecure.
- **Send** (`sock_ctx`, `buffer`, `length`, `out_sent_length`): may send fewer
  bytes than asked, setting `out_sent_length` to how many it sent; `khc` sends
  the rest in later calls.
- **Recv** (`sock_ctx`, `buffer`, `length_to_read`, `out_actual_length`): sets
  `out_actual_length` to how many bytes it read. 0 means the server closed the
  connection.
- **Close** (`sock_ctx`): closes the connection. Once connected, it is called
  when the request ends, whether it succeeded or not. It is not called when
  connecting fails.

Implementations to start from:

- [linux-sample](../tio/linux-sample/sock_cb_linux.c): BSD sockets and OpenSSL.
- [win32-sample](../tio/win32-sample/sock_cb_win32.c): Winsock and Schannel.

## Results

`khc_perform()` returns `KHC_ERR_OK` once a response has been read, whatever
its status: a `404` or a `500` is a response like any other, and
`khc_get_status_code()` gives its status. Other results:

| Result | Cause |
|---|---|
| `KHC_ERR_SOCK_CONNECT`, `KHC_ERR_SOCK_SEND`, `KHC_ERR_SOCK_RECV`, `KHC_ERR_SOCK_CLOSE` | A socket callback returned `KHC_SOCK_FAIL`. |
| `KHC_ERR_HEADER_CALLBACK` | The header callback handled fewer bytes than given, or the response's `Content-Length` or `Transfer-Encoding` is malformed, unsupported or too long for the response header buffer. |
| `KHC_ERR_WRITE_CALLBACK` | The write callback handled fewer bytes than given. |
| `KHC_ERR_ALLOCATION` | A buffer `khc` allocates could not be allocated. |
| `KHC_ERR_TOO_LARGE_DATA` | The status line doesn't fit in the response header buffer, or a chunk size line in the stream buffer. |
| `KHC_ERR_FAIL` | The status code or a chunk size is malformed, or the connection closed partway through a chunked body. |

## Tracing

Configuring with `-DKHC_TRACE=ON` prints every request and response to
stdout, headers and bodies included. That includes access tokens and
passwords, so never enable it where its output is kept or shared.

## Building

`khc` is built as part of ebisu, as described in the
[top-level README](../README.md#building). `libkii` includes it, and it is
also built on its own as `libkhc`.
