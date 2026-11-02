#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include <ebisu_socket.h>
#include "ebisu_version.h"

void khc_set_resp_header_buff(khc* khc, char* buffer, size_t buff_size) {
    khc->_resp_header_buff = buffer;
    khc->_resp_header_buff_size = buff_size;
}

void khc_set_stream_buff(khc* khc, char* buffer, size_t buff_size) {
    khc->_stream_buff = buffer;
    khc->_stream_buff_size = buff_size;
}

void khc_set_system(khc* khc, ebisu_system_t* system) {
    khc->_system = system;
}

void khc_set_timeout_ms(khc* khc, uint32_t timeout_ms) {
    khc->_timeout_ms = timeout_ms;
}

void khc_set_cb_continue(khc* khc, KHC_CB_CONTINUE cb, void* userdata) {
    khc->_cb_continue = cb;
    khc->_continue_data = userdata;
}

static uint64_t _now_ms(khc* khc) {
    return khc->_system->ops->monotonic_ms(khc->_system);
}

// Why the request is to stop before its next step, if it is: it took too long,
// or the continue callback cancelled it. KHC_ERR_OK if it goes on.
static khc_code _stop_reason(khc* khc) {
    if (khc->_timeout_ms > 0 &&
            _now_ms(khc) - khc->_started_ms >= khc->_timeout_ms) {
        return KHC_ERR_TIMEOUT;
    }
    if (khc->_cb_continue != NULL && !khc->_cb_continue(khc->_continue_data)) {
        return KHC_ERR_CANCELLED;
    }
    return KHC_ERR_OK;
}

// Stops the request for reason, closing the connection if one is open, as any
// failure does.
static void _stop(khc* khc, khc_code reason) {
    switch (khc->_state) {
        case KHC_STATE_IDLE:
        case KHC_STATE_CONNECT:
            // Not connected yet.
            khc->_result = reason;
            khc->_state = KHC_STATE_FINISHED;
            break;
        case KHC_STATE_CLOSE:
            // Closing, after the response or a failure, or after stopping, and
            // the close returned EBISU_SOCK_AGAIN: it is given up on. The
            // request keeps its result.
            khc->_close_failed = true;
            khc->_state = KHC_STATE_FINISHED;
            break;
        default:
            khc->_result = reason;
            khc->_state = KHC_STATE_CLOSE;
            break;
    }
}

khc_code khc_perform(khc* khc) {
    // Called without checking from here on. The read callback is left out of
    // this: a request need not have a body.
    const ebisu_socket_ops_t* ops = khc->_socket != NULL ? khc->_socket->ops : NULL;
    if (ops == NULL || ops->connect == NULL || ops->send == NULL ||
            ops->recv == NULL || ops->close == NULL ||
            khc->_cb_header == NULL || khc->_cb_write == NULL) {
        return KHC_ERR_FAIL;
    }
    // A timeout needs a clock to be measured with.
    if (khc->_timeout_ms > 0 && (khc->_system == NULL ||
            khc->_system->ops == NULL || khc->_system->ops->monotonic_ms == NULL)) {
        return KHC_ERR_FAIL;
    }
    const bool can_wait = khc->_system != NULL && khc->_system->ops != NULL &&
        khc->_system->ops->delay_ms != NULL;
    if (khc->_timeout_ms > 0) {
        khc->_started_ms = _now_ms(khc);
    }
    khc->_state = KHC_STATE_IDLE;
    khc->_result = KHC_ERR_OK;
    khc->_close_failed = false;
    khc->_resp_header_skipped = false;
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
        khc_code reason = _stop_reason(khc);
        if (reason != KHC_ERR_OK) {
            _stop(khc, reason);
            // Closing the connection, if there is one, is this step. Should
            // it return EBISU_SOCK_AGAIN, the next is given up on.
            if (khc->_state != KHC_STATE_CLOSE) {
                continue;
            }
        }
        khc->_again = false;
        state_handlers[khc->_state](khc);
        if (khc->_again && can_wait) {
            khc->_system->ops->delay_ms(khc->_system, KHC_AGAIN_WAIT_MS);
        }
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

    khc->_socket = NULL;

    khc->_cb_continue = NULL;
    khc->_continue_data = NULL;
    khc->_system = NULL;
    khc->_timeout_ms = 0;

    // User settings.
    khc->_enable_insecure = false;
    khc->_absolute_target = false;
    // A week's margin, for a committer whose clock was a little ahead.
    khc->_resp_date_floor = EBISU_SOURCE_TIME - 7 * 86400;

    khc_reset_except_cb(khc);
}

void khc_reset_except_cb(khc* khc) {
    khc->_req_headers = NULL;
    khc->_host[0] = '\0';
    khc->_path[0] = '\0';
    khc->_method[0] = '\0';
    khc->_req_body_length = KHC_REQ_BODY_LENGTH_UNKNOWN;

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
    khc->_req_body_left = 0;
    khc->_req_frame_start = 0;
    khc->_req_frame_size = 0;
    khc->_resp_header_read_size = 0;
    khc->_status_code =0;
    khc->_chunked_resp = false;
    khc->_chunk_size = 0;
    khc->_chunk_size_written = 0;
    khc->_resp_content_length = 0;
    khc->_resp_has_content_length = false;
    khc->_resp_body_received = 0;
    khc->_trailer_end_matched = 0;
    khc->_resp_date = -1;
    khc->_read_end = false;
    khc->_body_read_size = 0;
    khc->_result = KHC_ERR_OK;
    khc->_close_failed = false;
    khc->_resp_header_skipped = false;
    khc->_sent_length = 0;
    khc->_started_ms = 0;
    khc->_again = false;
    khc->_sock_error = EBISU_SOCK_ERROR_NONE;
    khc->_sock_error_detail = 0;
}

ebisu_sock_error_t khc_get_sock_error(khc* khc, long* out_detail) {
    if (out_detail != NULL) {
        *out_detail = khc->_sock_error_detail;
    }
    return khc->_sock_error;
}

void khc_set_req_body_length(khc* khc, size_t length) {
    khc->_req_body_length = length;
}

void khc_enable_insecure(
        khc* khc,
        bool enable_insecure) {
    khc->_enable_insecure = enable_insecure;
}

void khc_enable_absolute_target(
        khc* khc,
        bool enable_absolute_target) {
    khc->_absolute_target = enable_absolute_target;
}

uint16_t khc_get_status_code(
        khc* khc
) {
    return khc->_status_code;
}

bool khc_get_close_failed(khc* khc) {
    return khc->_close_failed;
}

bool khc_get_resp_header_skipped(khc* khc) {
    return khc->_resp_header_skipped;
}

int64_t khc_get_resp_date(khc* khc) {
    return (khc->_resp_date >= 0) ? khc->_resp_date : -1;
}
