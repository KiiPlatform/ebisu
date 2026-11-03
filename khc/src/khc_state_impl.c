#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include "khc_http_date.h"

#ifdef KHC_TRACE
static void _trace_dump(char prefix, char* buf, size_t len) {
    for(int i=0;i<len;i++) {
        if (buf[i] == '\r') {
            printf("\n%c", prefix);
        }
        else if (buf[i]=='\n'){
        }
        else {
            printf("%c", buf[i]);
        }

    }
}

static void _trace_op(char* op, int res) {
    printf("* %s = %d\n", op, res);
}
#endif

void khc_set_socket(khc* khc, ebisu_socket_t* socket)
{
    khc->_socket = socket;
}

khc_code khc_set_cb_read(
        khc* khc,
        KHC_CB_READ cb,
        void* userdata)
{
    khc->_cb_read = cb;
    khc->_read_data = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_cb_write(
        khc* khc,
        KHC_CB_WRITE cb,
        void* userdata)
{
    khc->_cb_write = cb;
    khc->_write_data = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_cb_header(
        khc* khc,
        KHC_CB_HEADER cb,
        void* userdata)
{
    khc->_cb_header = cb;
    khc->_header_data = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_host(khc* khc, const char* host) {
    size_t len = strlen(host);
    size_t buff_len = sizeof(khc->_host);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    memcpy(khc->_host, host, len + 1);
    return KHC_ERR_OK;
}

khc_code khc_set_path(khc* khc, const char* path) {
    size_t len = strlen(path);
    size_t buff_len = sizeof(khc->_path);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    memcpy(khc->_path, path, len + 1);
    return KHC_ERR_OK;
}

khc_code khc_set_method(khc* khc, const char* method) {
    size_t len = strlen(method);
    size_t buff_len = sizeof(khc->_method);
    if (buff_len < len + 1) {
        return KHC_ERR_TOO_LARGE_DATA;
    }
    memcpy(khc->_method, method, len + 1);
    return KHC_ERR_OK;
}

bool khc_method_can_have_body(khc* khc) {
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

khc_code khc_set_req_headers(khc* khc, khc_slist* headers) {
    khc->_req_headers = headers;
    return KHC_ERR_OK;
}

void khc_state_idle(khc* khc) {
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
static void _record_sock_error(khc* khc) {
    const ebisu_socket_ops_t* ops = khc->_socket->ops;
    khc->_sock_error_detail = 0;
    khc->_sock_error = (ops->last_error != NULL) ?
        ops->last_error(khc->_socket, &khc->_sock_error_detail) :
        EBISU_SOCK_ERROR_NONE;
}

// The connection closed before the response ended.
static void _closed_early(khc* khc) {
    khc->_sock_error = EBISU_SOCK_ERROR_CLOSED;
    khc->_sock_error_detail = 0;
}

void khc_state_connect(khc* khc) {
    const uint16_t port = khc->_enable_insecure ? 80:443;
    ebisu_sock_code_t con_res = khc->_socket->ops->connect(khc->_socket, khc->_host, port);
    #ifdef KHC_TRACE
    _trace_op("connect", con_res);
    #endif
    if (con_res == EBISU_SOCK_OK) {
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
static const char http_version[] = "HTTP/1.1\r\n";

// The path, as an empty one has to go out as "/".
static const char* request_path(khc* khc) {
    // Path must be started with '/'
    return khc->_path[0] != '\0' ? khc->_path : "/";
}

static size_t request_line_len(khc* khc) {
    const char* schema = khc->_enable_insecure ? schema_http : schema_https;
    size_t url_prefix_len = khc->_absolute_target ? strlen(schema) + strlen(khc->_host) : 0;
    return ( // example)GET /v1/users HTTP1.1\r\n
        strlen(khc->_method) + 1
        + url_prefix_len + strlen(request_path(khc)) + 1
        + strlen(http_version)
        );
}

void khc_state_req_line(khc* khc) {
    size_t len = request_line_len(khc);
    char request_line[len+1];

    request_line[0] = '\0';
    strcat(request_line, khc->_method);
    strcat(request_line, " ");
    // Otherwise the host goes in the Host header, which is all a server wants.
    if (khc->_absolute_target) {
        strcat(request_line, khc->_enable_insecure ? schema_http : schema_https);
        strcat(request_line, khc->_host);
    }
    strcat(request_line, request_path(khc));
    strcat(request_line, " ");
    strcat(request_line, http_version);
    char* send_pos = &request_line[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        khc->_state = KHC_STATE_REQ_HOST_HEADER;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_host_header(khc* khc) {
    const char hdr_key[] = "HOST: ";
    size_t hdr_len = strlen(hdr_key) + strlen(khc->_host) + 2;
    char buff[hdr_len + 1];
    snprintf(buff, hdr_len + 1, "%s%s\r\n", hdr_key, khc->_host);
    char* send_pos = &buff[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        khc->_state = KHC_STATE_REQ_HEADER;
        khc->_current_req_header = khc->_req_headers;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_header(khc* khc) {
    if (khc->_current_req_header == NULL) {
        khc->_state = KHC_STATE_REQ_HEADER_END;
        return;
    }
    char* data = khc->_current_req_header->data;
    if (data == NULL) {
        // Skip if data is NULL.
        khc->_current_req_header = khc->_current_req_header->next;
        return;
    }
    size_t len = strlen(data);
    if (len == 0) {
        // Skip if nothing to send.
        khc->_current_req_header = khc->_current_req_header->next;
        return;
    }
    khc->_state = KHC_STATE_REQ_HEADER_SEND;
}

void khc_state_req_header_send(khc* khc) {
    char* line = khc->_current_req_header->data;
    char* send_pos = &line[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        khc->_state = KHC_STATE_REQ_HEADER_SEND_CRLF;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_header_send_crlf(khc* khc) {
    char crlf[] = "\r\n";
    char* send_pos = &crlf[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        khc->_current_req_header = khc->_current_req_header->next;
        khc->_state = KHC_STATE_REQ_HEADER;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

// Whether the request has a body to read from the read callback.
static bool _has_req_body(khc* khc) {
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

void khc_state_req_header_end(khc* khc) {
    static const char connection_close[] = "Connection: Close\r\n\r\n";
    char text[sizeof("Content-Length: \r\n") + sizeof(size_t) * 3 + sizeof(connection_close)];
    text[0] = '\0';
    if(!khc_method_can_have_body(khc)) {
        // Nothing more than Connection.
    } else if(khc->_cb_read == NULL) {
        strcpy(text, "Content-Length: 0\r\n");
    } else if(khc->_req_body_length != KHC_REQ_BODY_LENGTH_UNKNOWN) {
        strcpy(text, "Content-Length: ");
        size_t len = strlen(text);
        len += _decimal(khc->_req_body_length, &text[len]);
        strcpy(&text[len], "\r\n");
    } else {
        strcpy(text, "Transfer-Encoding: chunked\r\n");
    }
    strcat(text, connection_close);
    char* send_pos = &text[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        // A method without a body ends the request here: its headers say
        // nothing of one, so a body, or even the chunk ending an empty one,
        // would be taken for the start of another request.
        if (_has_req_body(khc)) {
            khc->_req_body_left = khc->_req_body_length;
            khc->_state = KHC_STATE_REQ_BODY_READ;
        } else {
            khc->_resp_header_read_size = 0;
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        }
        khc->_read_req_end = false;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

static void _fail_req_body(khc* khc, khc_code result) {
    khc->_state = KHC_STATE_CLOSE;
    khc->_result = result;
}

// The number of hex digits of size.
static size_t _hex_digits(size_t size) {
    size_t digits = 1;
    while (size >>= 4) {
        ++digits;
    }
    return digits;
}

// Reads the next part of the body into the stream buffer, framed as it is to
// be sent, so that each part goes out in one send: over TLS, one record rather
// than three for each chunk.
void khc_state_req_body_read(khc* khc) {
    if (khc->_req_body_length != KHC_REQ_BODY_LENGTH_UNKNOWN) {
        size_t want = khc->_req_body_left < khc->_stream_buff_size ?
            khc->_req_body_left : khc->_stream_buff_size;
        size_t got = khc->_cb_read(khc->_stream_buff, want, khc->_read_data);
        if (got == 0 || got > want) {
            _fail_req_body(khc, KHC_ERR_READ_CALLBACK);
            return;
        }
        khc->_req_body_left -= got;
        khc->_read_size = got;
        khc->_read_req_end = khc->_req_body_left == 0;
        khc->_req_frame_start = 0;
        khc->_req_frame_size = got;
        khc->_state = KHC_STATE_REQ_BODY_SEND;
        return;
    }

    // Room for the longest size line a chunk that fits can have before the
    // data, and for CRLF after it.
    size_t line_room = _hex_digits(khc->_stream_buff_size) + 2;
    if (khc->_stream_buff_size <= line_room + 2) {
        _fail_req_body(khc, KHC_ERR_TOO_LARGE_DATA);
        return;
    }
    size_t capacity = khc->_stream_buff_size - line_room - 2;
    char* data = &khc->_stream_buff[line_room];
    size_t got = khc->_cb_read(data, capacity, khc->_read_data);
    if (got > capacity) {
        _fail_req_body(khc, KHC_ERR_READ_CALLBACK);
        return;
    }
    char line[KHC_CHUNK_SIZE_LINE_SIZE];
    size_t line_len = _khc_chunk_size_line(got, line);
    khc->_req_frame_start = line_room - line_len;
    memcpy(&khc->_stream_buff[khc->_req_frame_start], line, line_len);
    memcpy(&data[got], "\r\n", 2);
    khc->_req_frame_size = line_len + got + 2;
    khc->_read_size = got;
    // The chunk of size 0 ends the body.
    khc->_read_req_end = got == 0;
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

void khc_state_req_body_send(khc* khc) {
    char* send_pos = &khc->_stream_buff[khc->_req_frame_start + khc->_sent_length];
    size_t send_len = khc->_req_frame_size - khc->_sent_length;
    size_t sent_len = 0;
    ebisu_sock_code_t send_res = khc->_socket->ops->send(khc->_socket, send_pos, send_len, &sent_len);
    if (send_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('>', send_pos, sent_len);
        #endif
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        if (khc->_read_req_end) {
            khc->_resp_header_read_size = 0;
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        } else {
            khc->_state = KHC_STATE_REQ_BODY_READ;
        }
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("send", send_res);
    }
    #endif
    if (send_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (send_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
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

void khc_state_resp_status_read(khc* khc) {
    size_t read_size = 0;
    size_t read_req_size = khc->_resp_header_buff_size - khc->_resp_header_read_size - 1;
    ebisu_sock_code_t read_res =
        khc->_socket->ops->recv(khc->_socket, &khc->_resp_header_buff[khc->_resp_header_read_size], read_req_size, &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', &khc->_resp_header_buff[khc->_resp_header_read_size], read_size);
        #endif
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            khc->_read_end = true;
        }
        if (_find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size) != NULL) {
            khc->_state = KHC_STATE_RESP_STATUS_PARSE;
            return;
        }
        if (khc->_resp_header_read_size + 1 == khc->_resp_header_buff_size) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_TOO_LARGE_DATA;
        } else if (read_size == 0) {
            // Closed before the status line ended.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            _closed_early(khc);
        }
        // Otherwise a read can end anywhere: read the rest of the line.
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_status_parse(khc* khc) {
    const char http_version[] = "HTTP/d.d ";
    const size_t status_size = 3;
    const char* line_end = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
    // Check the line itself: past it, the buffer may hold what an earlier
    // response left there.
    if (line_end == NULL ||
            (size_t)(line_end - khc->_resp_header_buff) < strlen(http_version) + status_size ||
            strncmp(khc->_resp_header_buff, "HTTP/", 5) != 0) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_FAIL;
        return;
    }
    char* ptr = khc->_resp_header_buff + strlen(http_version);

    // Three digits, so at most 999.
    uint16_t status_code = 0;
    for (size_t i = 0; i < status_size; ++i) {
        char d = ptr[i];
        if (isdigit((unsigned char)d) == 0){
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_FAIL;
            return;
        }
        status_code = status_code * 10 + (d - '0');
    }
    khc->_status_code = status_code;
    // A response of its own, an interim 1xx one included: what framed the
    // one before doesn't frame this one.
    khc->_resp_date = -1;
    khc->_resp_content_length = 0;
    khc->_resp_has_content_length = false;
    khc->_chunked_resp = false;
    khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
    return;
}

void khc_state_resp_header_callback(khc* khc) {
    const char* header_boundary = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
    if (header_boundary == NULL) {
        if (khc->_resp_header_buff_size == khc->_resp_header_read_size + 1) {
            // no space in _resp_header_buff.
            khc->_resp_header_skipped = true;
            khc->_state = KHC_STATE_RESP_HEADER_SKIP;
            return;
        }
        khc->_state = KHC_STATE_RESP_HEADER_READ;
        return;
    }
    size_t header_size = header_boundary - khc->_resp_header_buff;
    if (header_size > 0) {
        size_t content_length = 0;
        size_t header_written = khc->_cb_header(khc->_resp_header_buff,
                header_size, khc->_header_data);
        if (header_written != header_size) { // Error in callback function.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_HEADER_CALLBACK;
            return;
        }
        _record_resp_date(khc, khc->_resp_header_buff, header_size);

        int chunked = _is_chunked_encoding(khc->_resp_header_buff, header_size);
        if (chunked == 1) {
            khc->_chunked_resp = true;
        } else if (chunked == -1) { // Transfer coding khc cannot remove.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_HEADER_CALLBACK;
            return;
        } else {
            int extracted = _extract_content_length(khc->_resp_header_buff, header_size, &content_length);
            if (extracted == 1 && khc->_resp_has_content_length &&
                    content_length != khc->_resp_content_length) {
                // Lengths that disagree: body length unknown.
                khc->_state = KHC_STATE_CLOSE;
                khc->_result = KHC_ERR_HEADER_CALLBACK;
                return;
            } else if (extracted == 1) {
                khc->_resp_content_length = content_length;
                khc->_resp_has_content_length = true;
            } else if (extracted == -1) { // Invalid Content-Length: body length unknown.
                khc->_state = KHC_STATE_CLOSE;
                khc->_result = KHC_ERR_HEADER_CALLBACK;
                return;
            }
        }
    }

    memmove(khc->_resp_header_buff, header_boundary + 2,
            khc->_resp_header_read_size - (header_size + 2) + 1); // +1 is '\0' include.
    khc->_resp_header_read_size -= (header_size + 2);

    if (header_size == 0) {
        if (100 <= khc->_status_code && khc->_status_code < 200) {
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        } else {
            if (strcmp(khc->_method, "HEAD") == 0 ||
                    khc->_status_code == 204 || khc->_status_code == 304) {
                // These have no body, whatever length they give.
                khc->_state = KHC_STATE_CLOSE;
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

void khc_state_resp_header_read(khc* khc) {
    size_t read_size = 0;
    size_t read_req_size = khc->_resp_header_buff_size - khc->_resp_header_read_size - 1;
    ebisu_sock_code_t read_res =
        khc->_socket->ops->recv(khc->_socket, &khc->_resp_header_buff[khc->_resp_header_read_size], read_req_size, &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', &khc->_resp_header_buff[khc->_resp_header_read_size], read_size);
        #endif
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            // Closed before the headers ended: nothing more will come.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            _closed_early(khc);
            return;
        }
        khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_header_skip(khc* khc) {
    // check required http headers.
    if (_is_header_present("Content-Length", khc->_resp_header_buff, khc->_resp_header_read_size) == 1 ||
            _is_header_present("Transfer-Encoding", khc->_resp_header_buff, khc->_resp_header_read_size) == 1) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_TOO_LARGE_DATA;
        return;
    }

    // check and leave CR at last.
    if (khc->_resp_header_buff[khc->_resp_header_read_size - 1] == '\r') {
        khc->_resp_header_buff[0] = '\r';
        khc->_resp_header_read_size = 1;
    } else {
        khc->_resp_header_read_size = 0;
    }

    size_t read_size = 0;
    size_t read_req_size = khc->_resp_header_buff_size - khc->_resp_header_read_size - 1;
    ebisu_sock_code_t read_res = khc->_socket->ops->recv(
            khc->_socket,
            &khc->_resp_header_buff[khc->_resp_header_read_size],
            read_req_size,
            &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', &khc->_resp_header_buff[khc->_resp_header_read_size], read_size);
        #endif
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            // Closed before the headers ended: nothing more will come.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            _closed_early(khc);
            return;
        }
        // check header boundary.
        const char* header_boundary = _find_crlf(khc->_resp_header_buff, khc->_resp_header_read_size);
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
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

/* A body that isn't chunked ends at the response's Content-Length, or without
 * one when the connection closes. If it closes short of that length, the body
 * ended early. */
static void _end_resp_body(khc* khc) {
    khc->_state = KHC_STATE_CLOSE;
    if (khc->_resp_body_received < khc->_resp_content_length) {
        khc->_result = KHC_ERR_SOCK_RECV;
        _closed_early(khc);
    }
}

static bool _resp_body_complete(const khc* khc) {
    return khc->_resp_has_content_length &&
        khc->_resp_body_received >= khc->_resp_content_length;
}

/* How much of size bytes just read is body: what comes after Content-Length
 * isn't. */
static size_t _resp_body_part(const khc* khc, size_t size) {
    if (!khc->_resp_has_content_length) {
        return size;
    }
    if (_resp_body_complete(khc)) {
        return 0;
    }
    size_t remain = khc->_resp_content_length - khc->_resp_body_received;
    return size < remain ? size : remain;
}

void khc_state_resp_body_fragment(khc* khc) {
    size_t size = _resp_body_part(khc, khc->_resp_header_read_size);
    if (size > 0) {
        size_t written = khc->_cb_write(khc->_resp_header_buff, size, khc->_write_data);
        if (written != size) { // Error in write callback.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_WRITE_CALLBACK;
            return;
        }
        khc->_resp_body_received += written;
    }

    if (khc->_read_end || _resp_body_complete(khc)) {
        _end_resp_body(khc);
        return;
    } else {
        khc->_state = KHC_STATE_RESP_BODY_READ;
        return;
    }
}

int _move_resp_header_to_stream(khc* khc) {
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

void khc_state_read_chunk_size_from_header_buff(khc* khc) {
    if (_move_resp_header_to_stream(khc) == 0) {
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
    } else {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_TOO_LARGE_DATA;
    }
}

void khc_state_read_chunk_body_from_header_buff(khc* khc) {
    if (_move_resp_header_to_stream(khc) == 0) {
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
    } else {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_TOO_LARGE_DATA;
    }
}

void khc_state_resp_body_read(khc* khc) {
    size_t read_size = 0;
    ebisu_sock_code_t read_res =
        khc->_socket->ops->recv(khc->_socket, khc->_stream_buff, khc->_stream_buff_size, &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', khc->_stream_buff, read_size);
        #endif
        khc->_body_read_size = read_size;
        if (read_size == 0) {
            khc->_read_end = true;
            _end_resp_body(khc);
            return;
        }
        khc->_state = KHC_STATE_RESP_BODY_CALLBACK;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_callback(khc* khc) {
    size_t size = _resp_body_part(khc, khc->_body_read_size);
    size_t written = khc->_cb_write(khc->_stream_buff, size, khc->_write_data);
    if (written < size) { // Error in write callback.
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_WRITE_CALLBACK;
        return;
    }
    khc->_resp_body_received += written;
    if (khc->_read_end || _resp_body_complete(khc)) {
        _end_resp_body(khc);
    } else {
        khc->_state = KHC_STATE_RESP_BODY_READ;
    }
    return;
}

void khc_state_resp_body_parse_chunk_size(khc* khc) {
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
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_FAIL;
    } else {
        if (khc->_resp_header_read_size > 0) {
            khc->_state = KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF;
        } else {
            khc->_state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;
        }
    }
}

void khc_state_resp_body_read_chunk_size(khc* khc) {
    size_t read_size = 0;
    long remain = khc->_stream_buff_size - khc->_body_read_size;
    if (remain <= 0) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_TOO_LARGE_DATA;
        return;
    }
    ebisu_sock_code_t read_res = khc->_socket->ops->recv(
            khc->_socket,
            &khc->_stream_buff[khc->_body_read_size],
            remain,
            &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', &khc->_stream_buff[khc->_body_read_size], read_size);
        #endif
        khc->_body_read_size += read_size;
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            _closed_early(khc);
            return;
        }
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_parse_chunk_body(khc* khc) {
    size_t chunk_remain = khc->_chunk_size - khc->_chunk_size_written;
    size_t target_size =
        chunk_remain > khc->_body_read_size ?
        khc->_body_read_size :
        chunk_remain;
    size_t written = khc->_cb_write(khc->_stream_buff, target_size, khc->_write_data);
    if (written < target_size) { // Error in write callback.
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_WRITE_CALLBACK;
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

void khc_state_resp_body_read_chunk_body(khc* khc) {
    size_t read_size = 0;
    ebisu_sock_code_t read_res = khc->_socket->ops->recv(
            khc->_socket,
            khc->_stream_buff,
            khc->_stream_buff_size,
            &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', khc->_stream_buff, read_size);
        #endif
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            _closed_early(khc);
            return;
        }
        khc->_body_read_size = read_size;
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_skip_chunk_body_crlf(khc* khc) {
    if (khc->_body_read_size < 2) {
        if (khc->_resp_header_read_size > 0) {
            // What was read along with the headers comes first.
            _move_resp_header_to_stream(khc);
            return;
        }
        size_t tmp_size = 2 - khc->_body_read_size;
        char tmp[tmp_size];
        size_t read_size = 0;
        ebisu_sock_code_t read_res = khc->_socket->ops->recv(
                khc->_socket,
                tmp,
                tmp_size,
                &read_size);
        if (read_res == EBISU_SOCK_OK) {
            #ifdef KHC_TRACE
            _trace_dump('<', tmp, read_size);
            #endif
            if (read_size == 0) {
                khc->_state = KHC_STATE_CLOSE;
                khc->_result = KHC_ERR_SOCK_RECV;
                _closed_early(khc);
                return;
            }
            khc->_body_read_size += read_size;
            if (khc->_body_read_size < 2) {
                // Only the CR has come: skip the LF on the next read.
                return;
            }
            khc->_body_read_size = 0;
            khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
            return;
        }
        #ifdef KHC_TRACE
        else {
            _trace_op("recv", read_res);
        }
        #endif
        if (read_res == EBISU_SOCK_AGAIN) {
            khc->_again = true;
            return;
        }
        if (read_res == EBISU_SOCK_FAIL) {
            _record_sock_error(khc);
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_SOCK_RECV;
            return;
        }
    } else {
        size_t remain = khc->_body_read_size - 2;
        memmove(khc->_stream_buff, &khc->_stream_buff[2], remain);
        khc->_body_read_size = remain;
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
    }
}

/* Whether data has the empty line that ends the trailers. */
static bool _find_trailer_end(khc* khc, const char* data, size_t size) {
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

void khc_state_resp_body_skip_trailers(khc* khc) {
    // What was read along with the last chunk comes first.
    if (_find_trailer_end(khc, khc->_stream_buff, khc->_body_read_size) ||
            _find_trailer_end(khc, khc->_resp_header_buff, khc->_resp_header_read_size)) {
        khc->_state = KHC_STATE_CLOSE;
        return;
    }
    khc->_body_read_size = 0;
    khc->_resp_header_read_size = 0;

    size_t read_size = 0;
    ebisu_sock_code_t read_res = khc->_socket->ops->recv(
            khc->_socket,
            khc->_stream_buff,
            khc->_stream_buff_size,
            &read_size);
    if (read_res == EBISU_SOCK_OK) {
        #ifdef KHC_TRACE
        _trace_dump('<', khc->_stream_buff, read_size);
        #endif
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            return;
        }
        khc->_body_read_size = read_size;
        return;
    }
    #ifdef KHC_TRACE
    else {
        _trace_op("recv", read_res);
    }
    #endif
    if (read_res == EBISU_SOCK_AGAIN) {
        khc->_again = true;
        return;
    }
    if (read_res == EBISU_SOCK_FAIL) {
        _record_sock_error(khc);
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_close(khc* khc) {
    ebisu_sock_code_t close_res = khc->_socket->ops->close(khc->_socket);
    #ifdef KHC_TRACE
    _trace_op("close", close_res);
    #endif
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
    khc_state_req_host_header,
    khc_state_req_header,
    khc_state_req_header_send,
    khc_state_req_header_send_crlf,
    khc_state_req_header_end,
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
