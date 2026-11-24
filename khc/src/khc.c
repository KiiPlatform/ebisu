#include <stdlib.h>
#include <string.h>
#include "khc.h"
#include "khc_impl.h"
#include "khc_state_impl.h"
#include <ebisu_mem.h>
#include <ebisu_socket.h>
#include "ebisu_version.h"

void khc_set_resp_header_buff(khc_t* khc, char* buffer, size_t buff_size) {
    khc->_resp_header_buff = buffer;
    khc->_resp_header_buff_size = buff_size;
}

void khc_set_stream_buff(khc_t* khc, char* buffer, size_t buff_size) {
    khc->_stream_buff = buffer;
    khc->_stream_buff_size = buff_size;
}

void khc_set_system(khc_t* khc, ebisu_system_t* system) {
    khc->_system = system;
}

void khc_set_timeout_ms(khc_t* khc, uint32_t timeout_ms) {
    khc->_timeout_ms = timeout_ms;
}

void khc_set_cb_continue(khc_t* khc, KHC_CB_CONTINUE cb, void* userdata) {
    khc->_cb_continue = cb;
    khc->_continue_data = userdata;
}

static uint64_t _now_ms(khc_t* khc) {
    return khc->_system->ops->monotonic_ms(khc->_system);
}

// Why the request is to stop before its next step, if it is: it took too long,
// or the continue callback cancelled it. KHC_ERR_OK if it goes on.
static khc_code_t _stop_reason(khc_t* khc) {
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
static void _stop(khc_t* khc, khc_code_t reason) {
    switch (khc->_state) {
        case KHC_STATE_IDLE:
            // Not connected yet.
            khc->_result = reason;
            khc->_state = KHC_STATE_FINISHED;
            break;
        case KHC_STATE_CONNECT:
            // A connect that returned EBISU_SOCK_AGAIN in the step before has
            // a connection on the way, which the socket keeps until it is
            // called again or closed: it is closed, as an open one is. Before
            // the first connect there is none.
            khc->_result = reason;
            khc->_state = khc->_again ? KHC_STATE_CLOSE : KHC_STATE_FINISHED;
            break;
        case KHC_STATE_CLOSE:
            // Closing, after the response or a failure, or after stopping.
            // The request keeps its result. If the close was called, and
            // returned EBISU_SOCK_AGAIN, it is given up on. If it wasn't yet,
            // the state reached in the step before, it is this step, once:
            // the connection is not left open.
            if (khc->_again) {
                khc->_close_failed = true;
                khc->_state = KHC_STATE_FINISHED;
            }
            break;
        default:
            khc->_result = reason;
            khc->_state = KHC_STATE_CLOSE;
            break;
    }
}

// Whether headers are lines, each with something before its CRLF, and no CR,
// LF or NUL elsewhere: a line end inside one would make it two headers, an
// empty line would end them, and a NUL is no header text.
static bool _headers_are_lines(const char* headers, size_t length) {
    if (length == 0) {
        return true;
    }
    if (headers == NULL || length < 2 || headers[length - 2] != '\r' ||
            headers[length - 1] != '\n') {
        return false;
    }
    size_t line_start = 0;
    for (size_t i = 0; i < length; ++i) {
        const char c = headers[i];
        if (c == '\0' || c == '\n') {
            return false;
        }
        if (c == '\r') {
            if (i + 1 >= length || headers[i + 1] != '\n' || i == line_start) {
                return false;
            }
            ++i;
            line_start = i + 1;
        }
    }
    return true;
}

khc_code_t khc_perform(khc_t* khc) {
    // Called without checking from here on. The read callback is left out of
    // this: a request need not have a body.
    const ebisu_socket_ops_t* ops = khc->_socket != NULL ? khc->_socket->ops : NULL;
    if (ops == NULL || ops->connect == NULL || ops->send == NULL ||
            ops->recv == NULL || ops->close == NULL ||
            khc->_cb_header == NULL || khc->_cb_write == NULL) {
        return KHC_ERR_FAIL;
    }
    // A buffer given with a size of 0. The room left in a response header
    // buffer, its size less one for the NUL, wrapped around, and reads into
    // it were no longer bounded by it. A stream buffer was read into with a
    // size of 0, which a socket answers as the end of the stream, ending a
    // body at what came with the headers. A NULL buffer is allocated, as
    // khc_init() leaves them.
    if ((khc->_resp_header_buff != NULL && khc->_resp_header_buff_size == 0) ||
            (khc->_stream_buff != NULL && khc->_stream_buff_size == 0)) {
        return KHC_ERR_FAIL;
    }
    // A timeout needs a clock to be measured with.
    if (khc->_timeout_ms > 0 && (khc->_system == NULL ||
            khc->_system->ops == NULL || khc->_system->ops->monotonic_ms == NULL)) {
        return KHC_ERR_FAIL;
    }
    if (!_headers_are_lines(khc->_req_headers, khc->_req_headers_length)) {
        return KHC_ERR_FAIL;
    }
    const bool can_wait = khc->_system != NULL && khc->_system->ops != NULL &&
        khc->_system->ops->delay_ms != NULL;
    // Each request starts from what the request is, not from what the last
    // one's response left behind. Left over, a Content-Length body's count of
    // the bytes received, or a close-delimited body's end of stream, ended the
    // next response's body early, with KHC_ERR_OK, unless
    // khc_reset_except_request() was called in between.
    khc_reset_except_request(khc);
    if (khc->_timeout_ms > 0) {
        khc->_started_ms = _now_ms(khc);
    }

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
        khc_code_t reason = _stop_reason(khc);
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

    // The stream buffer is cleared, given or allocated: the request's head
    // goes through it, its credentials among them, and its body, and the
    // response's. The allocated resources are freed, cleared first: a
    // response's headers pass through the header buffer.
    if (khc->_stream_buff != NULL) {
        ebisu_secure_zero(khc->_stream_buff, khc->_stream_buff_size);
    }
    if (khc->_stream_buff_allocated) {
        free(khc->_stream_buff);
        khc->_stream_buff = NULL;
        khc->_stream_buff_size = 0;
        khc->_stream_buff_allocated = false;
    }
    if (khc->_resp_header_buff_allocated) {
        ebisu_secure_zero(khc->_resp_header_buff, khc->_resp_header_buff_size);
        free(khc->_resp_header_buff);
        khc->_resp_header_buff = NULL;
        khc->_resp_header_buff_size = 0;
        khc->_resp_header_buff_allocated = false;
    }

    khc_code_t res = khc->_result;
    if (res != KHC_ERR_OK) {
        _khc_log(khc, EBISU_LOG_WARNING, "%s %s%s failed: %s", khc->_method, khc->_host,
                khc->_path, _khc_code_name(res));
    } else if (khc->_resp_body_received > 0) {
        _khc_log(khc, EBISU_LOG_DEBUG, "< (a body of %lu bytes)",
                (unsigned long)khc->_resp_body_received);
    }
    if (khc->_close_failed) {
        _khc_log(khc, EBISU_LOG_WARNING, "closing the connection to %s failed", khc->_host);
    }
    khc->_state = KHC_STATE_IDLE;
    khc->_result = KHC_ERR_OK;

    return res;
}

void khc_init(khc_t* khc) {
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
    khc->_logger = NULL;
    khc->_timeout_ms = 0;

    // User settings.
    khc->_enable_insecure = false;
    khc->_absolute_target = false;
    // A week's margin, for a committer whose clock was a little ahead.
    khc->_resp_date_floor = EBISU_SOURCE_TIME - 7 * 86400;

    khc_reset_except_cb(khc);
}

void khc_reset_except_cb(khc_t* khc) {
    khc->_req_headers = NULL;
    khc->_req_headers_length = 0;
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

void khc_reset_except_request(khc_t* khc) {
    // Internal states.
    khc->_state = KHC_STATE_IDLE;
    khc->_req_head_part = 0;
    khc->_req_head_offset = 0;
    khc->_read_req_end = false;
    khc->_req_body_cut = false;
    khc->_req_body_left = 0;
    khc->_req_body_sent = 0;
    khc->_req_frame_start = 0;
    khc->_req_frame_size = 0;
    khc->_resp_header_read_size = 0;
    khc->_status_code =0;
    khc->_resp_status_line_given = false;
    khc->_resp_version_1_0 = false;
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

ebisu_sock_error_t khc_get_sock_error(khc_t* khc, long* out_detail) {
    if (out_detail != NULL) {
        *out_detail = khc->_sock_error_detail;
    }
    return khc->_sock_error;
}

void khc_set_req_body_length(khc_t* khc, size_t length) {
    khc->_req_body_length = length;
}

void khc_enable_insecure(
        khc_t* khc,
        bool enable_insecure) {
    khc->_enable_insecure = enable_insecure;
}

void khc_enable_absolute_target(
        khc_t* khc,
        bool enable_absolute_target) {
    khc->_absolute_target = enable_absolute_target;
}

uint16_t khc_get_status_code(
        khc_t* khc
) {
    return khc->_status_code;
}

bool khc_get_close_failed(khc_t* khc) {
    return khc->_close_failed;
}

bool khc_get_resp_header_skipped(khc_t* khc) {
    return khc->_resp_header_skipped;
}

int64_t khc_get_resp_date(khc_t* khc) {
    return (khc->_resp_date >= 0) ? khc->_resp_date : -1;
}
