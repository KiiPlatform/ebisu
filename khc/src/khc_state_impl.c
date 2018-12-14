#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"

khc_code khc_set_cb_sock_connect(
        khc* khc,
        KHC_CB_SOCK_CONNECT cb,
        void* userdata)
{
    khc->_cb_sock_connect = cb;
    khc->_sock_ctx_connect = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_cb_sock_send(
        khc* khc,
        KHC_CB_SOCK_SEND cb,
        void* userdata)
{
    khc->_cb_sock_send = cb;
    khc->_sock_ctx_send = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_cb_sock_recv(
        khc* khc,
        KHC_CB_SOCK_RECV cb,
        void* userdata)
{
    khc->_cb_sock_recv = cb;
    khc->_sock_ctx_recv = userdata;
    return KHC_ERR_OK;
}

khc_code khc_set_cb_sock_close(
        khc* khc,
        KHC_CB_SOCK_CLOSE cb,
        void* userdata)
{
    khc->_cb_sock_close = cb;
    khc->_sock_ctx_close = userdata;
    return KHC_ERR_OK;
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
    if (khc->_resp_header_buff == NULL) {
        char* buff = malloc(DEFAULT_RESP_HEADER_BUFF_SIZE);
        if (buff == NULL) {
            khc->_state = KHC_STATE_FINISHED;
            khc->_result = KHC_ERR_ALLOCATION;
            return;
        }
        khc->_resp_header_buff = buff;
        khc->_resp_header_buff_allocated = 1;
        khc->_resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;
    }
    if (khc->_stream_buff == NULL) {
        char* buff = malloc(DEFAULT_STREAM_BUFF_SIZE);
        if (buff == NULL) {
            khc->_state = KHC_STATE_FINISHED;
            khc->_result = KHC_ERR_ALLOCATION;
            return;
        }
        khc->_stream_buff = buff;
        khc->_stream_buff_allocated = 1;
        khc->_stream_buff_size = DEFAULT_STREAM_BUFF_SIZE;
    }
    khc->_state = KHC_STATE_CONNECT;
    return;
}

void khc_state_connect(khc* khc) {
    khc_sock_code_t con_res = khc->_cb_sock_connect(khc->_sock_ctx_connect, khc->_host, 443);
    if (con_res == KHC_SOCK_OK) {
        khc->_sent_length = 0;
        khc->_state = KHC_STATE_REQ_LINE;
        return;
    }
    if (con_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (con_res == KHC_SOCK_FAIL) {
        khc->_result = KHC_ERR_SOCK_CONNECT;
        khc->_state = KHC_STATE_FINISHED;
        return;
    }
}

static const char schema[] = "https://";
static const char http_version[] = "HTTP/1.1\r\n";

static size_t request_line_len(khc* khc) {
    char* method = khc->_method;
    char* host = khc->_host;
    // Path must be started with '/'
    char* path = khc->_path;

    return ( // example)GET https://api.khc.com/v1/users HTTP1.1\r\n
        strlen(method) + 1
        + strlen(schema) + strlen(host) + strlen(path) + 1
        + strlen(http_version)
        );
}

void khc_state_req_line(khc* khc) {
    size_t len = request_line_len(khc);
    char request_line[len+1];
    char* host = khc->_host;
    char* path = khc->_path;

    request_line[0] = '\0';
    strcat(request_line, khc->_method);
    strcat(request_line, " ");
    strcat(request_line, schema);
    strcat(request_line, host);
    strcat(request_line, path);
    strcat(request_line, " ");
    strcat(request_line, http_version);
    char* send_pos = &request_line[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
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
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
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
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
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
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
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
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
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
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
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
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
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
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_header_end(khc* khc) {
    char* text = NULL;
    if (khc->_cb_read == NULL) {
        text = "Content-Length: 0\r\nConnection: Close\r\n\r\n";
    } else {
        text = "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n";
    }
    char* send_pos = &text[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        if (khc->_cb_read != NULL) {
            khc->_state = KHC_STATE_REQ_BODY_READ;
        } else {
            khc->_resp_header_read_size = 0;
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        }
        khc->_read_req_end = 0;
        return;
    }
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_body_read(khc* khc) {
    khc->_read_size = khc->_cb_read(khc->_stream_buff, khc->_stream_buff_size, khc->_read_data);
    if (khc->_read_size == 0) {
        khc->_read_req_end = 1;
    }
    khc->_state = KHC_STATE_REQ_BODY_SEND_SIZE;
    return;
}

void khc_state_req_body_send_size(khc* khc) {
    char size_buff[16];
    int size_len = snprintf(size_buff, 15, "%lx\r\n", khc->_read_size);
    if (size_len > 15) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_TOO_LARGE_DATA;
        return;
    }
    char* send_pos = &size_buff[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        if (khc->_read_req_end == 1) {
            khc->_state = KHC_STATE_REQ_BODY_SEND_CRLF;
        } else {
            khc->_state = KHC_STATE_REQ_BODY_SEND;
        }
        return;
    }
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_body_send(khc* khc) {
    char* send_pos = &khc->_stream_buff[khc->_sent_length];
    size_t send_len = khc->_read_size - khc->_sent_length;
    size_t sent_len = 0;
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        khc->_state = KHC_STATE_REQ_BODY_SEND_CRLF;
        return;
    }
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_req_body_send_crlf(khc* khc) {
    char crlf[] = "\r\n";
    char* send_pos = &crlf[khc->_sent_length];
    size_t send_len = strlen(send_pos);
    size_t sent_len = 0;
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, &sent_len);
    if (send_res == KHC_SOCK_OK) {
        if (sent_len < send_len) {
            khc->_sent_length += sent_len;
            // retry.
            return;
        } else {
            khc->_sent_length = 0;
        }
        if (khc->_read_req_end == 1) {
            khc->_resp_header_read_size = 0;
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        } else {
            khc->_state = KHC_STATE_REQ_BODY_READ;
        }
        return;
    }
    if (send_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (send_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_SEND;
        return;
    }
}

void khc_state_resp_status_read(khc* khc) {
    size_t read_size = 0;
    size_t read_req_size = khc->_resp_header_buff_size - khc->_resp_header_read_size - 1;
    khc_sock_code_t read_res =
        khc->_cb_sock_recv(khc->_sock_ctx_recv, &khc->_resp_header_buff[khc->_resp_header_read_size], read_req_size, &read_size);
    if (read_res == KHC_SOCK_OK) {
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            khc->_read_end = 1;
        }
        // check CRLF. no exist -> ERROR.
        if (strstr(khc->_resp_header_buff, "\r\n") == NULL) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_TOO_LARGE_DATA;
            return;
        }
        khc->_state = KHC_STATE_RESP_STATUS_PARSE;
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_status_parse(khc* khc) {
    const char http_version[] = "HTTP/d.d ";
    char* ptr = khc->_resp_header_buff + strlen(http_version);

    int status_code = 0;
    for (int i = 0; i < 3; ++i) {
        char d = ptr[i];
        if (isdigit((int)d) == 0){
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_FAIL;
            return;
        }
        status_code = status_code * 10 + (d - '0');
    }
    khc->_status_code = status_code;
    khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
    return;
}

void khc_state_resp_header_callback(khc* khc) {
    char* header_boundary = strstr(khc->_resp_header_buff, "\r\n");
    if (header_boundary == NULL) {
        if (khc->_resp_header_buff_size == khc->_resp_header_read_size + 1) {
            // no space in _resp_header_buff.
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

        if (_is_chunked_encoding(khc->_resp_header_buff, header_size) == 1) {
            khc->_chunked_resp = 1;
        } else if (_extract_content_length(khc->_resp_header_buff, header_size, &content_length) == 1) {
            khc->_resp_content_length = content_length;
        }
    }

    memmove(khc->_resp_header_buff, header_boundary + 2,
            khc->_resp_header_read_size - (header_size + 2) + 1); // +1 is '\0' include.
    khc->_resp_header_read_size -= (header_size + 2);

    if (header_size == 0) {
        if (100 <= khc->_status_code && khc->_status_code < 200) {
            khc->_state = KHC_STATE_RESP_STATUS_READ;
        } else {
            if (khc->_chunked_resp) {
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
    khc_sock_code_t read_res =
        khc->_cb_sock_recv(khc->_sock_ctx_recv, &khc->_resp_header_buff[khc->_resp_header_read_size], read_req_size, &read_size);
    if (read_res == KHC_SOCK_OK) {
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            khc->_read_end = 1;
        }
        khc->_state = KHC_STATE_RESP_HEADER_CALLBACK;
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
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
        khc->_result = KHC_ERR_HEADER_CALLBACK;
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
    khc_sock_code_t read_res = khc->_cb_sock_recv(
            khc->_sock_ctx_recv,
            &khc->_resp_header_buff[khc->_resp_header_read_size],
            read_req_size,
            &read_size);
    if (read_res == KHC_SOCK_OK) {
        khc->_resp_header_read_size += read_size;
        khc->_resp_header_buff[khc->_resp_header_read_size] = '\0';
        if (read_size == 0) {
            khc->_read_end = 1;
        }
        // check header boundary.
        char* header_boundary = strstr(khc->_resp_header_buff, "\r\n");
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
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_fragment(khc* khc) {
    if (khc->_resp_header_read_size > 0) {
        size_t written =
            khc->_cb_write(khc->_resp_header_buff, khc->_resp_header_read_size, khc->_write_data);
        if (written != khc->_resp_header_read_size) { // Error in write callback.
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_WRITE_CALLBACK;
            return;
        }
    }

    if (khc->_read_end == 1) {
        khc->_state = KHC_STATE_CLOSE;
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
    khc_sock_code_t read_res =
        khc->_cb_sock_recv(khc->_sock_ctx_recv, khc->_stream_buff, khc->_stream_buff_size, &read_size);
    if (read_res == KHC_SOCK_OK) {
        khc->_body_read_size = read_size;
        if (read_size == 0) {
            khc->_read_end = 1;
            khc->_state = KHC_STATE_CLOSE;
            return;
        }
        khc->_state = KHC_STATE_RESP_BODY_CALLBACK;
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_callback(khc* khc) {
    size_t written = khc->_cb_write(khc->_stream_buff, khc->_body_read_size, khc->_write_data);
    if (written < khc->_body_read_size) { // Error in write callback.
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_WRITE_CALLBACK;
        return;
    }
    if (khc->_read_end == 1) {
        khc->_state = KHC_STATE_CLOSE;
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
        const char* chunk_end = strstr(khc->_stream_buff, "\r\n");
        size_t remain = khc->_body_read_size - (chunk_end + 2 - khc->_stream_buff);
        memmove(khc->_stream_buff, chunk_end + 2, remain);
        khc->_body_read_size = remain;
        if (chunk_size > 0) {
            khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
        } else {
            khc->_state = KHC_STATE_RESP_BODY_SKIP_TRAILERS;
        }
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
    khc_sock_code_t read_res = khc->_cb_sock_recv(
            khc->_sock_ctx_recv,
            &khc->_stream_buff[khc->_body_read_size],
            remain,
            &read_size);
    if (read_res == KHC_SOCK_OK) {
        khc->_body_read_size += read_size;
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_FAIL;
            return;
        }
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_parse_chunk_body(khc* khc) {
    size_t target_size =
        khc->_chunk_size > khc->_body_read_size + khc->_chunk_size_written ?
        khc->_body_read_size :
        khc->_chunk_size - khc->_chunk_size_written;
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
    khc_sock_code_t read_res = khc->_cb_sock_recv(
            khc->_sock_ctx_recv,
            khc->_stream_buff,
            khc->_stream_buff_size,
            &read_size);
    if (read_res == KHC_SOCK_OK) {
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            khc->_result = KHC_ERR_FAIL;
            return;
        }
        khc->_body_read_size = read_size;
        khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_resp_body_skip_chunk_body_crlf(khc* khc) {
    if (khc->_body_read_size < 2) {
        size_t tmp_size = 2 - khc->_body_read_size;
        char tmp[tmp_size];
        size_t read_size = 0;
        khc_sock_code_t read_res = khc->_cb_sock_recv(
                khc->_sock_ctx_recv,
                tmp,
                tmp_size,
                &read_size);
        if (read_res == KHC_SOCK_OK) {
            if (read_size == 0) {
                khc->_state = KHC_STATE_CLOSE;
                khc->_result = KHC_ERR_FAIL;
                return;
            }
            khc->_body_read_size = 0;
            khc->_state = KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE;
            return;
        }
        if (read_res == KHC_SOCK_AGAIN) {
            return;
        }
        if (read_res == KHC_SOCK_FAIL) {
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

void khc_state_resp_body_skip_trailers(khc* khc) {
    size_t read_size = 0;
    khc_sock_code_t read_res = khc->_cb_sock_recv(
            khc->_sock_ctx_recv,
            khc->_stream_buff,
            khc->_stream_buff_size,
            &read_size);
    if (read_res == KHC_SOCK_OK) {
        if (read_size == 0) {
            khc->_state = KHC_STATE_CLOSE;
            return;
        }
        return;
    }
    if (read_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (read_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_CLOSE;
        khc->_result = KHC_ERR_SOCK_RECV;
        return;
    }
}

void khc_state_close(khc* khc) {
    if (khc->_stream_buff_allocated == 1) {
        free(khc->_stream_buff);
        khc->_stream_buff = NULL;
        khc->_stream_buff_size = 0;
        khc->_stream_buff_allocated = 0;
    }
    if (khc->_resp_header_buff_allocated == 1) {
        free(khc->_resp_header_buff);
        khc->_resp_header_buff = NULL;
        khc->_resp_header_buff_size = 0;
        khc->_resp_header_buff_allocated = 0;
    }
    khc_sock_code_t close_res = khc->_cb_sock_close(khc->_sock_ctx_close);
    if (close_res == KHC_SOCK_OK) {
        khc->_state = KHC_STATE_FINISHED;
        return;
    }
    if (close_res == KHC_SOCK_AGAIN) {
        return;
    }
    if (close_res == KHC_SOCK_FAIL) {
        khc->_state = KHC_STATE_FINISHED;
        khc->_result = KHC_ERR_SOCK_CLOSE;
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
    khc_state_req_body_send_size,
    khc_state_req_body_send,
    khc_state_req_body_send_crlf,

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
