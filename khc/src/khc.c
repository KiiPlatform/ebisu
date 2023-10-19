#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include "khc_socket_callback.h"

void khc_set_resp_header_buff(khc* khc, char* buffer, size_t buff_size) {
    khc->_resp_header_buff = buffer;
    khc->_resp_header_buff_size = buff_size;
}

void khc_set_stream_buff(khc* khc, char* buffer, size_t buff_size) {
    khc->_stream_buff = buffer;
    khc->_stream_buff_size = buff_size;
}

khc_code khc_perform(khc* khc) {
    khc->_state = KHC_STATE_IDLE;
    khc->_result = KHC_ERR_OK;

    // malloc the necessary resources
    if (khc->_resp_header_buff == NULL) {
        char* buff = malloc(DEFAULT_RESP_HEADER_BUFF_SIZE);
        if (buff == NULL) {
            return  KHC_ERR_ALLOCATION;
        }
        khc->_resp_header_buff = buff;
        khc->_resp_header_buff_allocated = 1;
        khc->_resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;
    }
    if (khc->_stream_buff == NULL) {
        char* buff = malloc(DEFAULT_STREAM_BUFF_SIZE);
        if (buff == NULL) {
            if (khc->_resp_header_buff_allocated == 1) {
                free(khc->_resp_header_buff);
                khc->_resp_header_buff = NULL;
                khc->_resp_header_buff_size = 0;
                khc->_resp_header_buff_allocated = 0;
            }
            return KHC_ERR_ALLOCATION;
        }
        khc->_stream_buff = buff;
        khc->_stream_buff_allocated = 1;
        khc->_stream_buff_size = DEFAULT_STREAM_BUFF_SIZE;
    }

    while(khc->_state != KHC_STATE_FINISHED) {
        state_handlers[khc->_state](khc);
    }

    // free the allocated resource
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

    khc_code res = khc->_result;
    khc->_state = KHC_STATE_IDLE;
    khc->_result = KHC_ERR_OK;

    return res;
}

void khc_init(khc* khc) {
    // Callbacks.
    khc->_cb_write = NULL;
    khc->_write_data = NULL;

    khc->_cb_header = NULL;
    khc->_header_data = NULL;

    khc->_cb_read = NULL;
    khc->_read_data = NULL;

    khc->_cb_sock_connect = NULL;
    khc->_sock_ctx_connect = NULL;

    khc->_cb_sock_send = NULL;
    khc->_sock_ctx_send = NULL;

    khc->_cb_sock_recv = NULL;
    khc->_sock_ctx_recv = NULL;

    khc->_cb_sock_close = NULL;
    khc->_sock_ctx_close = NULL;

    // User settings.
    khc->_enable_insecure = 0;

    khc_reset_except_cb(khc);
}

void khc_reset_except_cb(khc* khc) {
    khc->_req_headers = NULL;
    khc->_host[0] = '\0';
    khc->_path[0] = '\0';
    khc->_method[0] = '\0';

    // Internal states.
    khc->_state = KHC_STATE_IDLE;
    khc->_current_req_header = NULL;
    khc->_read_size = 0;
    khc->_read_req_end = 0;
    khc->_resp_header_read_size = 0;
    khc->_status_code =0;
    khc->_body_boundary = NULL;
    khc->_cb_header_pos = NULL;
    khc->_cb_header_remaining_size = 0;
    khc->_body_fragment = NULL;
    khc->_body_fragment_size = 0;
    khc->_chunked_resp = 0;
    khc->_chunk_size = 0;
    khc->_chunk_size_written = 0;
    khc->_resp_content_length = 0;
    khc->_read_end = 0;
    khc->_body_read_size = 0;
    khc->_result = KHC_ERR_OK;
    khc->_sent_length = 0;

    // Response header Buffer
    khc->_resp_header_buff = NULL;
    khc->_resp_header_buff_size = 0;
    khc->_resp_header_buff_allocated = 0;
    // Stream Buffer
    khc->_stream_buff = NULL;
    khc->_stream_buff_size = 0;
    khc->_stream_buff_allocated = 0;
}

void khc_enable_insecure(
        khc* khc,
        int enable_insecure) {
    khc->_enable_insecure = enable_insecure;
}

int khc_get_status_code(
        khc* khc
) {
    return khc->_status_code;
}

khc_sock_code_t _khc_sock_send(khc* khc,  char* send_pos, size_t send_len, size_t* sent_len) {
    khc_sock_code_t send_res = khc->_cb_sock_send(khc->_sock_ctx_send, send_pos, send_len, sent_len);
    #ifdef KHC_TRACE
    if(send_res==KHC_SOCK_OK) {
        _trace_dump('>', send_pos, *sent_len);
    }
    #endif
    return send_res;
}

khc_sock_code_t _khc_sock_recv(khc* khc, char* recv_pos, size_t recv_len, size_t* read_len) {
    khc_sock_code_t recv_res = khc->_cb_sock_recv(khc->_sock_ctx_recv, recv_pos, recv_len, read_len);
    #ifdef KHC_TRACE
    if(recv_res==KHC_SOCK_OK) {
        _trace_dump('<', recv_pos, *read_len);
    }
    else {

    }
    #endif
    return recv_res;
}

khc_sock_code_t _khc_sock_connect(khc* khc, unsigned int port) {
    khc_sock_code_t res = khc->_cb_sock_connect(khc->_sock_ctx_connect, khc->_host, port);
    _trace_op("connect", res);
    return res;
}

khc_sock_code_t _khc_sock_close(khc* khc) {
    khc_sock_code_t res = khc->_cb_sock_close(khc->_sock_ctx_close);
    _trace_op("close", res);
    return res;
}