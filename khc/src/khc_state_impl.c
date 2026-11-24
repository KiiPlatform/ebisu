#include <string.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include "khc_http_date.h"
#include <ebisu_str.h>


void khc_set_socket(khc_t* khc, ebisu_socket_t* socket)
{
    khc->_socket = socket;
}

khc_code_t khc_set_cb_read(
        khc_t* khc,
        KHC_CB_READ cb,
        void* userdata)
{
    khc->_cb_read = cb;
    khc->_read_data = userdata;
    return KHC_ERR_OK;
}

khc_code_t khc_set_cb_write(
        khc_t* khc,
        KHC_CB_WRITE cb,
        void* userdata)
{
    khc->_cb_write = cb;
    khc->_write_data = userdata;
    return KHC_ERR_OK;
}

khc_code_t khc_set_cb_header(
        khc_t* khc,
        KHC_CB_HEADER cb,
        void* userdata)
{
    khc->_cb_header = cb;
    khc->_header_data = userdata;
    return KHC_ERR_OK;
}

// Whether text can stand in the request line, or be the Host header's value,
// and leave the request as it was meant: with no space, which ends a part of
// the request line, no CR or LF, which end the line, and no other control
// character.
static bool _is_request_text(const char* text, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (c <= 0x20 || c == 0x7f) {
            return false;
        }
    }
    return true;
}

khc_code_t khc_set_host(khc_t* khc, const char* host) {
    size_t len = strlen(host);
    size_t buff_len = sizeof(khc->_host);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    if (!_is_request_text(host, len)) {
        return KHC_ERR_FAIL;
    }
    memcpy(khc->_host, host, len + 1);
    return KHC_ERR_OK;
}

khc_code_t khc_set_path(khc_t* khc, const char* path) {
    size_t len = strlen(path);
    size_t buff_len = sizeof(khc->_path);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    if (!_is_request_text(path, len)) {
        return KHC_ERR_FAIL;
    }
    memcpy(khc->_path, path, len + 1);
    return KHC_ERR_OK;
}

khc_code_t khc_set_method(khc_t* khc, const char* method) {
    size_t len = strlen(method);
    size_t buff_len = sizeof(khc->_method);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    if (!_is_request_text(method, len)) {
        return KHC_ERR_FAIL;
    }
    memcpy(khc->_method, method, len + 1);
    return KHC_ERR_OK;
}

bool khc_method_can_have_body(khc_t* khc) {
    if(khc->_method[0]==0) {
        return true; // undefined method.
    }
    if(strcmp(khc->_method, "GET") == 0) {
        return false;
    }
    if(strcmp(khc->_method, "HEAD") == 0) {
        return false;
    }
    if(strcmp(khc->_method, "DELETE") == 0) {
        return false;
    }
    return true;
}

khc_code_t khc_set_req_headers(khc_t* khc, const char* headers, size_t length) {
    khc->_req_headers = headers;
    khc->_req_headers_length = (headers != NULL) ? length : 0;
    return KHC_ERR_OK;
}

void khc_state_idle(khc_t* khc) {
    if (strlen(khc->_host) == 0) {
        // Fallback to localhost
        strncpy(khc->_host, "localhost", sizeof(khc->_host));
    }
    if (strlen(khc->_method) == 0) {
        // Fallback to GET.
        strncpy(khc->_method, "GET", sizeof(khc->_method));
    }
    khc->_state = KHC_STATE_CONNECT;
    return;
}

// Asks the socket why its operation failed, before anything else is done
// with it, closing it included.
static void _record_sock_error(khc_t* khc) {
    const ebisu_socket_ops_t* ops = khc->_socket->ops;
    khc->_sock_error_detail = 0;
    khc->_sock_error = (ops->last_error != NULL) ?
        ops->last_error(khc->_socket, &khc->_sock_error_detail) :
        EBISU_SOCK_ERROR_NONE;
}

// Ends the request with result: what is left is closing the connection.
static void _fail(khc_t* khc, khc_code_t result) {
    khc->_state = KHC_STATE_CLOSE;
    khc->_result = result;
}

// The connection closed before the response ended.
static void _closed_early(khc_t* khc) {
    _fail(khc, KHC_ERR_SOCK_RECV);
    khc->_sock_error = EBISU_SOCK_ERROR_CLOSED;
    khc->_sock_error_detail = 0;
}

// Receives up to size bytes into buffer: true with what came, none at the end
// of the stream; false with nothing to go on with, the socket not ready, which
// the request waits for, or failed, which ends it.
static bool _recv(khc_t* khc, char* buffer, size_t size, size_t* out_size) {
    *out_size = 0;
    ebisu_sock_code_t res = khc->_socket->ops->recv(khc->_socket, buffer, size, out_size);
    if (res == EBISU_SOCK_OK) {
        return true;
    }
    if (res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return false;
    }
    _record_sock_error(khc);
    _fail(khc, KHC_ERR_SOCK_RECV);
    return false;
}

// Receives into the response header buffer, after what it holds, which stays
// NUL-terminated, as _recv() does.
static bool _recv_into_header_buff(khc_t* khc, size_t* out_size) {
    size_t held = khc->_resp_header_read_size;
    if (!_recv(khc, &khc->_resp_header_buff[held], khc->_resp_header_buff_size - held - 1,
            out_size)) {
        return false;
    }
    khc->_resp_header_read_size += *out_size;
    khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
    return true;
}

void khc_state_connect(khc_t* khc) {
    const uint16_t port = khc->_enable_insecure ? 80:443;
    ebisu_sock_code_t con_res = khc->_socket->ops->connect(khc->_socket, khc->_host, port);
    if (con_res == EBISU_SOCK_OK) {
        _khc_log(khc, EBISU_LOG_DEBUG, "connected to %s:%u", khc->_host, (unsigned)port);
        khc->_sent_length = 0;
        khc->_state = KHC_STATE_REQ_LINE;
        return;
    }
    if (con_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (con_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_result = KHC_ERR_SOCK_CONNECT;
        khc->_state = KHC_STATE_FINISHED;
        return;
    }
}

static const char schema_https[] = "https://";
static const char schema_http[] = "http://";
static const char http_version[] = " HTTP/1.1\r\n";
static const char host_key[] = "HOST: ";
static const char crlf[] = "\r\n";

// The path, as an empty one has to go out as "/".
static const char* request_path(khc_t* khc) {
    // Path must be started with '/'
    return khc->_path[0] != '\0' ? khc->_path : "/";
}

// Whether the server has sent something, while the request is still being
// sent, as the socket's recv_wait says without waiting. It returns true only
// for what recv has to return: data, the end of the stream or a failure, a
// TLS record without data not counting. Without it, khc sends the whole
// request before it reads.
static bool _response_waiting(khc_t* khc) {
    const ebisu_socket_ops_t* ops = khc->_socket->ops;
    return ops->recv_wait != NULL && ops->recv_wait(khc->_socket, 0);
}

// A send failed: the server may have answered and closed the connection,
// the answer having arrived before the send found it closed. If the socket
// has it to return, it is the response, rather than the failure.
static void _send_failed(khc_t* khc) {
    _record_sock_error(khc);
    if (_response_waiting(khc)) {
        _khc_log(khc, EBISU_LOG_DEBUG,
                "sending failed with %lu bytes of the body sent: reading the server's answer",
                (unsigned long)khc->_req_body_sent);
        khc->_req_body_cut = true;
        khc->_state = KHC_STATE_RESP_STATUS_READ;
        return;
    }
    _fail(khc, KHC_ERR_SOCK_SEND);
}

// Whether the request has a body to read from the read callback.
static bool _has_req_body(khc_t* khc) {
    return khc_method_can_have_body(khc) && khc->_cb_read != NULL &&
        khc->_req_body_length != 0;
}

// Written by hand, as the chunk size line is.
static size_t _decimal(size_t value, char* out) {
    char digits[sizeof(size_t) * 3];
    size_t digits_len = 0;
    do {
        digits[digits_len++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    size_t len = 0;
    while (digits_len > 0) {
        out[len++] = digits[--digits_len];
    }
    out[len] = '\0';
    return len;
}

static const char connection_close[] = "Connection: Close\r\n\r\n";

// The size of the head's end: the body's length, or that it is chunked, and
// Connection, with the empty line that ends the head.
#define HEAD_END_SIZE \
    (sizeof("Content-Length: \r\n") + sizeof(size_t) * 3 + sizeof(connection_close))

// What ends the head, after the application's headers.
static void _head_end(khc_t* khc, char out[HEAD_END_SIZE]) {
    out[0] = '\0';
    if(!khc_method_can_have_body(khc)) {
        // Nothing more than Connection.
    } else if(khc->_cb_read == NULL) {
        strcpy(out, "Content-Length: 0\r\n");
    } else if(khc->_req_body_length != KHC_REQ_BODY_LENGTH_UNKNOWN) {
        strcpy(out, "Content-Length: ");
        size_t len = strlen(out);
        len += _decimal(khc->_req_body_length, &out[len]);
        strcpy(&out[len], "\r\n");
    } else {
        strcpy(out, "Transfer-Encoding: chunked\r\n");
    }
    strcat(out, connection_close);
}

// The parts of the request's head, in the order they are sent: the request
// line, the Host header, the application's headers, and the end.
enum {
    HEAD_METHOD,
    HEAD_SPACE,
    HEAD_SCHEME,
    HEAD_TARGET_HOST,
    HEAD_PATH,
    HEAD_VERSION,
    HEAD_HOST_KEY,
    HEAD_HOST,
    HEAD_HOST_CRLF,
    HEAD_HEADERS,
    HEAD_END,
    HEAD_DONE
};

// The scheme and host before the path, for an absolute target. Otherwise the
// host goes in the Host header only, which is all a server wants.
static const char* _target_scheme(khc_t* khc) {
    if (!khc->_absolute_target) {
        return "";
    }
    return khc->_enable_insecure ? schema_http : schema_https;
}

static const char* _target_host(khc_t* khc) {
    return khc->_absolute_target ? khc->_host : "";
}

// The part of the head khc is at, and its length.
static const char* _head_part(khc_t* khc, const char* end, size_t* out_len) {
    if (khc->_req_head_part == HEAD_HEADERS && khc->_req_headers != NULL) {
        *out_len = khc->_req_headers_length;
        return khc->_req_headers;
    }
    const char* part;
    switch (khc->_req_head_part) {
        case HEAD_METHOD: part = khc->_method; break;
        case HEAD_SPACE: part = " "; break;
        case HEAD_SCHEME: part = _target_scheme(khc); break;
        case HEAD_TARGET_HOST: part = _target_host(khc); break;
        case HEAD_PATH: part = request_path(khc); break;
        case HEAD_VERSION: part = http_version; break;
        case HEAD_HOST_KEY: part = host_key; break;
        case HEAD_HOST: part = khc->_host; break;
        case HEAD_HOST_CRLF: part = crlf; break;
        case HEAD_END: part = end; break;
        default: part = ""; break;
    }
    *out_len = strlen(part);
    return part;
}

// Moves on to the part of the head after the one khc is at.
static void _next_head_part(khc_t* khc) {
    ++khc->_req_head_part;
    khc->_req_head_offset = 0;
}

// Logs the head, a line at a time, without the CRLFs.
static void _log_head(khc_t* khc, const char* end) {
#ifndef EBISU_NO_LOG
    if (khc->_logger == NULL) {
        return;
    }
    _khc_log(khc, EBISU_LOG_DEBUG, "> %s %s%s%s HTTP/1.1", khc->_method,
            _target_scheme(khc), _target_host(khc), request_path(khc));
    _khc_log(khc, EBISU_LOG_DEBUG, "> %s%s", host_key, khc->_host);
    _khc_log_lines(khc, '>', khc->_req_headers, khc->_req_headers_length);
    _khc_log_lines(khc, '>', end, strlen(end));
#else
    (void)khc;
    (void)end;
#endif
}

// Puts the request's head in the stream buffer, to be sent with what fits of
// the body after it. Sent a line at a time, each line went in a send of its
// own, over TLS a record of its own, and a socket without TCP_NODELAY held
// back each after the first until the server acknowledged it, which a server
// may delay: by 40 ms on Linux. A head larger than the buffer goes a buffer
// at a time.
void khc_state_req_line(khc_t* khc) {
    char end[HEAD_END_SIZE];
    _head_end(khc, end);
    char* buff = khc->_stream_buff;
    size_t size = khc->_stream_buff_size;
    size_t used = 0;
    while (khc->_req_head_part != HEAD_DONE && used < size) {
        size_t len;
        const char* part = _head_part(khc, end, &len);
        // At most the part's length, which it never passes.
        size_t offset = (khc->_req_head_offset < len) ? khc->_req_head_offset : len;
        size_t left = len - offset;
        size_t n = (left < size - used) ? left : size - used;
        memcpy(&buff[used], &part[offset], n);
        used += n;
        khc->_req_head_offset = offset + n;
        if (n == left) {
            _next_head_part(khc);
        }
    }
    khc->_req_frame_start = 0;
    khc->_req_frame_size = used;
    khc->_sent_length = 0;
    khc->_state = KHC_STATE_REQ_BODY_SEND;
    if (khc->_req_head_part != HEAD_DONE) {
        return;
    }
    _log_head(khc, end);
    // A method without a body ends the request here: its headers say
    // nothing of one, so a body, or even the chunk ending an empty one,
    // would be taken for the start of another request.
    if (_has_req_body(khc)) {
        khc->_req_body_left = khc->_req_body_length;
        khc->_req_body_sent = 0;
        khc->_read_req_end = false;
        khc->_state = KHC_STATE_REQ_BODY_READ;
    } else {
        khc->_read_req_end = true;
    }
}

// The number of hex digits of size.
static size_t _hex_digits(size_t size) {
    size_t digits = 1;
    while (size >>= 4) {
        ++digits;
    }
    return digits;
}

// Reads the body into the stream buffer, after what of the head waits there,
// until the buffer is full or the body ends, framed as it is to be sent, so
// that the buffer goes out in one send: over TLS, one record, a chunk's size
// line and CRLF in it, and the chunk that ends the body too, should it fit.
void khc_state_req_body_read(khc_t* khc) {
    // What of the head waits to be sent, with this.
    size_t used = khc->_req_frame_size;
    // A server may answer before the body is all sent, a 401 or a 413 for
    // instance, and close the connection: RFC 9112, section 9.5, has the
    // client look for that while it sends, and stop. Sent on regardless, the
    // body met a closed connection, and the answer was lost with it. Before
    // the head is sent, there is nothing to answer.
    if (used == 0 && _response_waiting(khc)) {
        _khc_log(khc, EBISU_LOG_DEBUG,
                "the server answered with %lu bytes of the body sent: reading it",
                (unsigned long)khc->_req_body_sent);
        khc->_req_body_cut = true;
        khc->_state = KHC_STATE_RESP_STATUS_READ;
        return;
    }
    char* buff = khc->_stream_buff;
    size_t size = khc->_stream_buff_size;
    if (khc->_req_body_length != KHC_REQ_BODY_LENGTH_UNKNOWN) {
        while (khc->_req_body_left > 0 && used < size) {
            size_t want = (khc->_req_body_left < size - used) ?
                khc->_req_body_left : size - used;
            size_t got = khc->_cb_read(&buff[used], want, khc->_read_data);
            if (got == 0 || got > want) {
                _fail(khc, KHC_ERR_READ_CALLBACK);
                return;
            }
            khc->_req_body_left -= got;
            khc->_req_body_sent += got;
            used += got;
        }
        khc->_read_req_end = khc->_req_body_left == 0;
    } else {
        while (!khc->_read_req_end) {
            // Room for the longest size line a chunk that fits can have
            // before the data, and for CRLF after it.
            size_t room = size - used;
            size_t line_room = _hex_digits(room) + 2;
            if (room <= line_room + 2) {
                if (used == 0) {
                    _fail(khc, KHC_ERR_TOO_LARGE_DATA);
                    return;
                }
                // The next chunk goes in the next send.
                break;
            }
            size_t capacity = room - line_room - 2;
            char* data = &buff[used + line_room];
            size_t got = khc->_cb_read(data, capacity, khc->_read_data);
            if (got > capacity) {
                _fail(khc, KHC_ERR_READ_CALLBACK);
                return;
            }
            // The size line right after what is there, then the data.
            char line[KHC_CHUNK_SIZE_LINE_SIZE];
            size_t line_len = _khc_chunk_size_line(got, line);
            memmove(&buff[used + line_len], data, got);
            memcpy(&buff[used], line, line_len);
            memcpy(&buff[used + line_len + got], "\r\n", 2);
            used += line_len + got + 2;
            khc->_req_body_sent += got;
            // The chunk of size 0 ends the body.
            khc->_read_req_end = got == 0;
        }
    }
    khc->_req_frame_start = 0;
    khc->_req_frame_size = used;
    khc->_sent_length = 0;
    khc->_state = KHC_STATE_REQ_BODY_SEND;
}

// Written by hand rather than with snprintf: no printf length modifier matches
// size_t on every target and libc, and the buffer fits any size_t, so the line
// is never cut short.
size_t _khc_chunk_size_line(size_t size, char out[KHC_CHUNK_SIZE_LINE_SIZE]) {
    static const char hex_digits[] = "0123456789abcdef";
    char digits[sizeof(size_t) * 2];
    size_t digits_len = 0;
    do {
        digits[digits_len++] = hex_digits[size & 0xf];
        size >>= 4;
    } while (size != 0);

    size_t len = 0;
    while (digits_len > 0) {
        out[len++] = digits[--digits_len];
    }
    out[len++] = '\r';
    out[len++] = '\n';
    out[len] = '\0';
    return len;
}

// Sends what the stream buffer holds: the head, the body, or both.
void khc_state_req_body_send(khc_t* khc) {
    char* send_pos = &khc->_stream_buff[khc->_req_frame_start + khc->_sent_length];
    size_t send_len = khc->_req_frame_size - khc->_sent_length;
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        }
        khc->_sent_length = 0;
        khc->_req_frame_start = 0;
        khc->_req_frame_size = 0;
        if (khc->_req_head_part != HEAD_DONE) {
            khc->_state = KHC_STATE_REQ_LINE;
        } else if (!khc->_read_req_end) {
            khc->_state = KHC_STATE_REQ_BODY_READ;
        } else {
            if (_has_req_body(khc)) {
                _khc_log(khc, EBISU_LOG_DEBUG, "> (a body of %lu bytes)",
                        (unsigned long)khc->_req_body_sent);
            }
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        }
        return;
    }
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _send_failed(khc);
        return;
    }
}

// The first CRLF within the first len bytes of buff, or NULL. By length,
// not with strstr(): a NUL among what was read would hide the rest of it.
static const char* _find_crlf(const char* buff, size_t len) {
    for (size_t i = 0; i + 1 < len; ++i) {
        if (buff[i] == '\r' && buff[i + 1] == '\n') {
            return &buff[i];
        }
    }
    return NULL;
}

void khc_state_resp_status_read(khc_t* khc) {
    size_t read_size = 0;
    if (!_recv_into_header_buff(khc, &read_size)) {
        return;
    }
    if (read_size == 0) {
        khc->_read_end = true;
    }
    if (_find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size) != NULL) {
        khc->_state = KHC_STATE_RESP_STATUS_PARSE;
        return;
    }
    if (khc->_resp_header_read_size + 1 == khc->_resp_header_buff_size) {
        _fail(khc, KHC_ERR_TOO_LARGE_DATA);
    } else if (read_size == 0) {
        // Closed before the status line ended.
        _closed_early(khc);
    }
    // Otherwise a read can end anywhere: read the rest of the line.
}

void khc_state_resp_status_parse(khc_t* khc) {
    const char* line_end = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
    // Check the line itself: past it, the buffer may hold what an earlier
    // response left there.
    if (line_end == NULL ||
            !_is_status_line(khc->_resp_header_buff, (size_t)(line_end - khc->_resp_header_buff))) {
        _fail(khc, KHC_ERR_INVALID_RESPONSE);
        return;
    }
    // The three digits after "HTTP/1.1 ", so at most 999.
    const char* digits = khc->_resp_header_buff + 9;
    khc->_status_code = (uint16_t)((digits[0] - '0') * 100 + (digits[1] - '0') * 10 + (digits[2] - '0'));
    khc->_resp_status_line_given = false;
    khc->_resp_version_1_0 = khc->_resp_header_buff[7] == '0';
    // A response of its own, an interim 1xx one included: what framed the
    // one before doesn't frame this one.
    khc->_resp_date = -1;
    khc->_resp_content_length = 0;
    khc->_resp_has_content_length = false;
    khc->_chunked_resp = false;
    khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
    return;
}

void khc_state_resp_header_callback(khc_t* khc) {
    const char* header_boundary = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
    if (header_boundary == NULL) {
        if (khc->_resp_header_buff_size == khc->_resp_header_read_size + 1) {
            // no space in _resp_header_buff. What there is of the header is
            // checked now, as what comes after it is while it is skipped.
            if (!_is_header_start(khc->_resp_header_buff, khc->_resp_header_read_size)) {
                _fail(khc, KHC_ERR_INVALID_RESPONSE);
                return;
            }
            khc->_resp_header_skipped = true;
            khc->_state = KHC_STATE_RESP_HEADER_SKIP;
            return;
        }
        khc->_state = KHC_STATE_RESP_HEADER_READ;
        return;
    }
    size_t header_size = header_boundary - khc->_resp_header_buff;
    // The status line, checked already, comes first, and the headers after
    // it. A line that starts with a space or a tab, which continues the one
    // before in the obsolete line folding, isn't one.
    if (header_size > 0 && khc->_resp_status_line_given &&
            !_is_header_line(khc->_resp_header_buff, header_size)) {
        _fail(khc, KHC_ERR_INVALID_RESPONSE);
        return;
    }
    if (header_size > 0) {
        size_t content_length = 0;
        size_t header_written = khc->_cb_header(khc->_resp_header_buff,
                header_size, khc->_header_data);
        if (header_written != header_size) { // Error in callback function.
            _fail(khc, KHC_ERR_HEADER_CALLBACK);
            return;
        }
        khc->_resp_status_line_given = true;
        _khc_log_header(khc, '<', khc->_resp_header_buff, header_size);
        _record_resp_date(khc, khc->_resp_header_buff, header_size);

        int chunked = _is_chunked_encoding(khc->_resp_header_buff, header_size);
        if (chunked == 1) {
            khc->_chunked_resp = true;
        } else if (chunked == -1) { // Transfer coding khc cannot remove.
            _fail(khc, KHC_ERR_INVALID_RESPONSE);
            return;
        } else {
            int extracted = _extract_content_length(khc->_resp_header_buff, header_size, &content_length);
            if (extracted == 1 && khc->_resp_has_content_length &&
                    content_length != khc->_resp_content_length) {
                // Lengths that disagree: body length unknown.
                _fail(khc, KHC_ERR_INVALID_RESPONSE);
                return;
            } else if (extracted == 1) {
                khc->_resp_content_length = content_length;
                khc->_resp_has_content_length = true;
            } else if (extracted == -1) { // Invalid Content-Length: body length unknown.
                _fail(khc, KHC_ERR_INVALID_RESPONSE);
                return;
            }
        }
    }

    memmove(khc->_resp_header_buff, header_boundary + 2,
            khc->_resp_header_read_size - (header_size + 2) + 1); // +1 is '\0' include.
    khc->_resp_header_read_size -= (header_size + 2);

    if (header_size == 0) {
        if (100 <= khc->_status_code && khc->_status_code < 200) {
            // An interim response that came before the body was sent whole,
            // with nothing of another after it: the server waits for the rest
            // of the body, which is sent before the final response is read,
            // from where sending stopped: what the stream buffer still holds
            // after a send that failed, or the next of the body.
            if (khc->_req_body_cut && khc->_resp_header_read_size == 0) {
                khc->_req_body_cut = false;
                khc->_state = (khc->_req_frame_size > 0) ?
                    KHC_STATE_REQ_BODY_SEND : KHC_STATE_REQ_BODY_READ;
                return;
            }
            // The final response may have come with this one, its status
            // line whole: then there is nothing to wait for.
            if (_find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size) != NULL) {
                khc->_state = KHC_STATE_RESP_STATUS_PARSE;
            } else {
                khc->_state = KHC_STATE_RESP_STATUS_READ;
            }
        } else {
            if (strcmp(khc->_method, "HEAD") == 0 ||
                    khc->_status_code == 204 || khc->_status_code == 304) {
                // These have no body, whatever length they give.
                khc->_state = KHC_STATE_CLOSE;
            } else if (khc->_chunked_resp &&
                    (khc->_resp_has_content_length || khc->_resp_version_1_0)) {
                // Chunks with a Content-Length too, or in an HTTP/1.0 response,
                // which has none: framing that something else on the way may
                // have read another way (RFC 9112, sections 6.1 and 6.3).
                _fail(khc, KHC_ERR_INVALID_RESPONSE);
            } else if (khc->_chunked_resp) {
                khc->_body_read_size = 0;
                khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
            } else {
                khc->_state = KHC_STATE_RESP_BODY_FRAGMENT;
            }
            return;
        }
    }
}

void khc_state_resp_header_read(khc_t* khc) {
    size_t read_size = 0;
    if (!_recv_into_header_buff(khc, &read_size)) {
        return;
    }
    if (read_size == 0) {
        // Closed before the headers ended: nothing more will come.
        _closed_early(khc);
        return;
    }
    khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
}

void khc_state_resp_header_skip(khc_t* khc) {
    // check required http headers.
    if (_is_header_present("Content-Length", khc->_resp_header_buff, khc->_resp_header_read_size) == 1 ||
            _is_header_present("Transfer-Encoding", khc->_resp_header_buff, khc->_resp_header_read_size) == 1) {
        _fail(khc, KHC_ERR_TOO_LARGE_DATA);
        return;
    }

    // check and leave CR at last. There may be nothing left of the last
    // read: this is called again when the socket had nothing to read yet.
    if (khc->_resp_header_read_size > 0 &&
            khc->_resp_header_buff[khc->_resp_header_read_size - 1] == '\r') {
        khc->_resp_header_buff[0] = '\r';
        khc->_resp_header_read_size = 1;
    } else {
        khc->_resp_header_read_size = 0;
    }

    size_t read_size = 0;
    if (!_recv_into_header_buff(khc, &read_size)) {
        return;
    }
    if (read_size == 0) {
        // Closed before the headers ended: nothing more will come.
        _closed_early(khc);
        return;
    }
    // check header boundary.
    const char* header_boundary = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
    // The rest of the value, up to its CRLF, or a CR that may start it.
    size_t value_size = header_boundary != NULL ?
        (size_t)(header_boundary - khc->_resp_header_buff) :
        khc->_resp_header_read_size;
    if (header_boundary == NULL && khc->_resp_header_buff[value_size - 1] == '\r') {
        --value_size;
    }
    if (!_is_field_text(khc->_resp_header_buff, value_size)) {
        _fail(khc, KHC_ERR_INVALID_RESPONSE);
        return;
    }
    if (header_boundary == NULL) {
        // re-skip.
        return;
    }
    // discard skip target.
    size_t header_size = header_boundary - khc->_resp_header_buff;
    memmove(khc->_resp_header_buff, header_boundary + 2,
            khc->_resp_header_read_size - (header_size + 2) + 1); // +1 is '\0' include.
    khc->_resp_header_read_size -= (header_size + 2);

    khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
}

/* Gives the write callback size bytes of the body, counted as received: true
 * if it took them all. Neither less nor more than it was given, as
 * KHC_CB_WRITE says: taken, more counted as received, and ended a
 * Content-Length body before it had arrived whole, with KHC_ERR_OK. */
static bool _write_body(khc_t* khc, char* data, size_t size) {
    if (khc->_cb_write(data, size, khc->_write_data) != size) {
        _fail(khc, KHC_ERR_WRITE_CALLBACK);
        return false;
    }
    khc->_resp_body_received += size;
    return true;
}

/* A body that isn't chunked ends at the response's Content-Length, or without
 * one when the connection closes. If it closes short of that length, the body
 * ended early. */
static void _end_resp_body(khc_t* khc) {
    khc->_state = KHC_STATE_CLOSE;
    if (khc->_resp_body_received < khc->_resp_content_length) {
        _closed_early(khc);
    }
}

static bool _resp_body_complete(const khc_t* khc) {
    return khc->_resp_has_content_length &&
        khc->_resp_body_received >= khc->_resp_content_length;
}

/* How much of size bytes just read is body: what comes after Content-Length
 * isn't. */
static size_t _resp_body_part(const khc_t* khc, size_t size) {
    if (!khc->_resp_has_content_length) {
        return size;
    }
    if (_resp_body_complete(khc)) {
        return 0;
    }
    size_t remain = khc->_resp_content_length - khc->_resp_body_received;
    return size < remain ? size : remain;
}

void khc_state_resp_body_fragment(khc_t* khc) {
    size_t size = _resp_body_part(khc, khc->_resp_header_read_size);
    if (size > 0 && !_write_body(khc, khc->_resp_header_buff, size)) {
        return;
    }

    if (khc->_read_end || _resp_body_complete(khc)) {
        _end_resp_body(khc);
        return;
    } else {
        khc->_state = KHC_STATE_RESP_BODY_READ;
        return;
    }
}

int _move_resp_header_to_stream(khc_t* khc) {
    long remain = khc->_stream_buff_size - khc->_body_read_size;
    if (remain <= 0) {
        return 1;
    }
    size_t move_size = remain > khc->_resp_header_read_size ? khc->_resp_header_read_size : remain;
    memmove(
            &khc->_stream_buff[khc->_body_read_size],
            khc->_resp_header_buff,
            move_size);
    khc->_body_read_size += move_size;
    khc->_resp_header_read_size -= move_size;
    memmove(
            khc->_resp_header_buff,
            &khc->_resp_header_buff[move_size],
            khc->_resp_header_read_size + 1);// move with '\0'
    return 0;
}

void khc_state_read_chunk_size_from_header_buff(khc_t* khc) {
    if (_move_resp_header_to_stream(khc) == 0) {
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
    } else {
        _fail(khc, KHC_ERR_TOO_LARGE_DATA);
    }
}

void khc_state_read_chunk_body_from_header_buff(khc_t* khc) {
    if (_move_resp_header_to_stream(khc) == 0) {
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
    } else {
        _fail(khc, KHC_ERR_TOO_LARGE_DATA);
    }
}

void khc_state_resp_body_read(khc_t* khc) {
    size_t read_size = 0;
    if (!_recv(khc, khc->_stream_buff, khc->_stream_buff_size, &read_size)) {
        return;
    }
    khc->_body_read_size = read_size;
    if (read_size == 0) {
        khc->_read_end = true;
        _end_resp_body(khc);
        return;
    }
    khc->_state = KHC_STATE_RESP_BODY_CALLBACK;
}

void khc_state_resp_body_callback(khc_t* khc) {
    size_t size = _resp_body_part(khc, khc->_body_read_size);
    if (!_write_body(khc, khc->_stream_buff, size)) {
        return;
    }
    if (khc->_read_end || _resp_body_complete(khc)) {
        _end_resp_body(khc);
    } else {
        khc->_state = KHC_STATE_RESP_BODY_READ;
    }
    return;
}

void khc_state_resp_body_parse_chunk_size(khc_t* khc) {
    size_t chunk_size = 0;
    int has_chunk_size = _read_chunk_size(khc->_stream_buff, khc->_body_read_size, &chunk_size);
    if (has_chunk_size == 1) {
        khc->_chunk_size = chunk_size;
        khc->_chunk_size_written = 0;
        // The CRLF _read_chunk_size() found, which ends the size line. Not
        // with strstr(): the stream buffer holds what has been read, with no
        // terminator after it, so it would read on past the end.
        const char* chunk_end = _find_crlf(khc->_stream_buff, khc->_body_read_size);
        size_t remain = khc->_body_read_size - (chunk_end + 2 - khc->_stream_buff);
        memmove(khc->_stream_buff, chunk_end + 2, remain);
        khc->_body_read_size = remain;
        if (chunk_size > 0) {
            khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
        } else {
            // The CRLF of this last chunk's line starts what ends the body.
            khc->_trailer_end_matched = 2;
            khc->_state = KHC_STATE_RESP_BODY_SKIP_TRAILERS;
        }
    } else if (has_chunk_size < 0) {
        // Without a valid size the rest of the body can't be framed.
        _fail(khc, KHC_ERR_INVALID_RESPONSE);
    } else {
        if (khc->_resp_header_read_size > 0) {
            khc->_state = KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF;
        } else {
            khc->_state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;
        }
    }
}

void khc_state_resp_body_read_chunk_size(khc_t* khc) {
    // A size line that fills the buffer is too long to read.
    if (khc->_body_read_size >= khc->_stream_buff_size) {
        _fail(khc, KHC_ERR_TOO_LARGE_DATA);
        return;
    }
    size_t read_size = 0;
    if (!_recv(khc, &khc->_stream_buff[khc->_body_read_size],
            khc->_stream_buff_size - khc->_body_read_size, &read_size)) {
        return;
    }
    khc->_body_read_size += read_size;
    if (read_size == 0) {
        _closed_early(khc);
        return;
    }
    khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
}

void khc_state_resp_body_parse_chunk_body(khc_t* khc) {
    size_t chunk_remain = khc->_chunk_size - khc->_chunk_size_written;
    size_t target_size =
        chunk_remain > khc->_body_read_size ?
        khc->_body_read_size :
        chunk_remain;
    if (!_write_body(khc, khc->_stream_buff, target_size)) {
        return;
    }
    khc->_chunk_size_written += target_size;
    if (khc->_chunk_size > khc->_chunk_size_written) {
        if (khc->_resp_header_read_size > 0) {
            khc->_body_read_size = 0;
            khc->_state = KHC_STATE_READ_CHUNK_BODY_FROM_HEADER_BUFF;
        } else {
            khc->_state = KHC_STATE_RESP_BODY_READ_CHUNK_BODY;
        }
    } else {
        size_t remain = khc->_body_read_size - target_size;
        memmove(khc->_stream_buff, &khc->_stream_buff[target_size], remain);
        khc->_body_read_size = remain;
        khc->_state = KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF;
        return;
    }
    return;
}

void khc_state_resp_body_read_chunk_body(khc_t* khc) {
    size_t read_size = 0;
    if (!_recv(khc, khc->_stream_buff, khc->_stream_buff_size, &read_size)) {
        return;
    }
    if (read_size == 0) {
        _closed_early(khc);
        return;
    }
    khc->_body_read_size = read_size;
    khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
}

// The CRLF that ends a chunk's data, of which the first size bytes of data
// have come.
static bool _is_chunk_data_end(const char* data, size_t size) {
    return memcmp(data, "\r\n", size) == 0;
}

void khc_state_resp_body_skip_chunk_body_crlf(khc_t* khc) {
    if (!_is_chunk_data_end(khc->_stream_buff, khc->_body_read_size < 2 ? khc->_body_read_size : 2)) {
        // Something else after the data: the chunk's size didn't say where
        // it ended.
        _fail(khc, KHC_ERR_INVALID_RESPONSE);
        return;
    }
    if (khc->_body_read_size < 2) {
        if (khc->_resp_header_read_size > 0) {
            // What was read along with the headers comes first.
            _move_resp_header_to_stream(khc);
            return;
        }
        size_t tmp_size = 2 - khc->_body_read_size;
        char tmp[2];
        size_t read_size = 0;
        if (!_recv(khc, tmp, tmp_size, &read_size)) {
            return;
        }
        if (read_size == 0) {
            _closed_early(khc);
            return;
        }
        // What came has to be what was left of the CRLF.
        if (memcmp(tmp, &"\r\n"[khc->_body_read_size], read_size) != 0) {
            _fail(khc, KHC_ERR_INVALID_RESPONSE);
            return;
        }
        khc->_body_read_size += read_size;
        if (khc->_body_read_size < 2) {
            // Only the CR has come: it is kept, for the check above, and
            // the LF read next.
            khc->_stream_buff[0] = '\r';
            return;
        }
        khc->_body_read_size = 0;
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
    } else {
        size_t remain = khc->_body_read_size - 2;
        memmove(khc->_stream_buff, &khc->_stream_buff[2], remain);
        khc->_body_read_size = remain;
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
    }
}

/* Whether data has the empty line that ends the trailers. */
static bool _find_trailer_end(khc_t* khc, const char* data, size_t size) {
    const char trailer_end[] = "\r\n\r\n";
    for (size_t i = 0; i < size; ++i) {
        if (data[i] == trailer_end[khc->_trailer_end_matched]) {
            ++khc->_trailer_end_matched;
            if (khc->_trailer_end_matched == sizeof(trailer_end) - 1) {
                return true;
            }
        } else {
            khc->_trailer_end_matched = data[i] == '\r' ? 1 : 0;
        }
    }
    return false;
}

void khc_state_resp_body_skip_trailers(khc_t* khc) {
    // What was read along with the last chunk comes first.
    if (_find_trailer_end(khc, khc->_stream_buff, khc->_body_read_size) ||
            _find_trailer_end(khc, khc->_resp_header_buff, khc->_resp_header_read_size)) {
        khc->_state = KHC_STATE_CLOSE;
        return;
    }
    khc->_body_read_size = 0;
    khc->_resp_header_read_size = 0;

    size_t read_size = 0;
    if (!_recv(khc, khc->_stream_buff, khc->_stream_buff_size, &read_size)) {
        return;
    }
    if (read_size == 0) {
        khc->_state = KHC_STATE_CLOSE;
        return;
    }
    khc->_body_read_size = read_size;
}

void khc_state_close(khc_t* khc) {
    ebisu_sock_code_t close_res = khc->_socket->ops->close(khc->_socket);
    if (close_res == EBISU_SOCK_OK) {
        khc->_state = KHC_STATE_FINISHED;
        return;
    }
    if (close_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (close_res == EBISU_SOCK_FAIL) {
        // The request has its result by now, and this must not change it. After
        // a whole response, a failure would make a POST the server has acted on
        // look as if it had not been; after a failure, it would hide what went
        // wrong.
        khc->_state = KHC_STATE_FINISHED;
        khc->_close_failed = true;
        return;
    }
}

const KHC_STATE_HANDLER state_handlers[] = {
    khc_state_idle,
    khc_state_connect,
    khc_state_req_line,
    // KHC_STATE_REQ_HOST_HEADER to KHC_STATE_REQ_HEADER_END, which are no
    // longer entered.
    khc_state_req_body_send,
    khc_state_req_body_send,
    khc_state_req_body_send,
    khc_state_req_body_send,
    khc_state_req_body_send,
    khc_state_req_body_read,
    // KHC_STATE_REQ_BODY_SEND_SIZE, which is no longer entered.
    khc_state_req_body_send,
    khc_state_req_body_send,
    // KHC_STATE_REQ_BODY_SEND_CRLF, which is no longer entered.
    khc_state_req_body_send,

    khc_state_resp_status_read,
    khc_state_resp_status_parse,
    khc_state_resp_header_callback,
    khc_state_resp_header_read,
    khc_state_resp_header_skip,
    khc_state_resp_body_fragment,
    khc_state_read_chunk_size_from_header_buff,
    khc_state_read_chunk_body_from_header_buff,

    khc_state_resp_body_read,
    khc_state_resp_body_callback,

    khc_state_resp_body_parse_chunk_size,
    khc_state_resp_body_read_chunk_size,
    khc_state_resp_body_parse_chunk_body,
    khc_state_resp_body_read_chunk_body,
    khc_state_resp_body_skip_chunk_body_crlf,
    khc_state_resp_body_skip_trailers,

    khc_state_close
};
