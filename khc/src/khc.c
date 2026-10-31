#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include "khc_socket_callback.h"
#include "ebisu_version.h"

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
    // Only this request's response may set it.
    khc->_resp_date = -1;

    // malloc the necessary resources
    if (khc->_resp_header_buff == NULL) {
        char* buff = malloc(DEFAULT_RESP_HEADER_BUFF_SIZE);
        if (buff == NULL) {
            return  KHC_ERR_ALLOCATION;
        }
        khc->_resp_header_buff = buff;
        khc->_resp_header_buff_allocated = true;
        khc->_resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;
    }
    if (khc->_stream_buff == NULL) {
        char* buff = malloc(DEFAULT_STREAM_BUFF_SIZE);
        if (buff == NULL) {
            if (khc->_resp_header_buff_allocated) {
                free(khc->_resp_header_buff);
                khc->_resp_header_buff = NULL;
                khc->_resp_header_buff_size = 0;
                khc->_resp_header_buff_allocated = false;
            }
            return KHC_ERR_ALLOCATION;
        }
        khc->_stream_buff = buff;
        khc->_stream_buff_allocated = true;
        khc->_stream_buff_size = DEFAULT_STREAM_BUFF_SIZE;
    }

    while(khc->_state != KHC_STATE_FINISHED) {
        state_handlers[khc->_state](khc);
    }

    // free the allocated resource
    if (khc->_stream_buff_allocated) {
        free(khc->_stream_buff);
        khc->_stream_buff = NULL;
        khc->_stream_buff_size = 0;
        khc->_stream_buff_allocated = false;
    }
    if (khc->_resp_header_buff_allocated) {
        free(khc->_resp_header_buff);
        khc->_resp_header_buff = NULL;
        khc->_resp_header_buff_size = 0;
        khc->_resp_header_buff_allocated = false;
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
    khc->_enable_insecure = false;
    // A week's margin, for a committer whose clock was a little ahead.
    khc->_resp_date_floor = EBISU_SOURCE_TIME - 7 * 86400;

    khc_reset_except_cb(khc);
}

void khc_reset_except_cb(khc* khc) {
    khc->_req_headers = NULL;
    khc->_host[0] = '\0';
    khc->_path[0] = '\0';
    khc->_method[0] = '\0';

    khc_reset_except_request(khc);

    // Response header Buffer
    khc->_resp_header_buff = NULL;
    khc->_resp_header_buff_size = 0;
    khc->_resp_header_buff_allocated = false;
    // Stream Buffer
    khc->_stream_buff = NULL;
    khc->_stream_buff_size = 0;
    khc->_stream_buff_allocated = false;
}

void khc_reset_except_request(khc* khc) {
    // Internal states.
    khc->_state = KHC_STATE_IDLE;
    khc->_current_req_header = NULL;
    khc->_read_size = 0;
    khc->_read_req_end = false;
    khc->_resp_header_read_size = 0;
    khc->_status_code =0;
    khc->_chunked_resp = false;
    khc->_chunk_size = 0;
    khc->_chunk_size_written = 0;
    khc->_resp_content_length = 0;
    khc->_resp_body_received = 0;
    khc->_resp_date = -1;
    khc->_read_end = false;
    khc->_body_read_size = 0;
    khc->_result = KHC_ERR_OK;
    khc->_sent_length = 0;
}

void khc_enable_insecure(
        khc* khc,
        bool enable_insecure) {
    khc->_enable_insecure = enable_insecure;
}

uint16_t khc_get_status_code(
        khc* khc
) {
    return khc->_status_code;
}

int64_t khc_get_resp_date(khc* khc) {
    return (khc->_resp_date >= 0) ? khc->_resp_date : -1;
}
