#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "khc.h"
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
    while(khc->_state != KHC_STATE_FINISHED) {
        state_handlers[khc->_state](khc);
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

int khc_get_status_code(
        khc* khc
) {
    return khc->_status_code;
}
