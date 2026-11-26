#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "kii_session_impl.h"
#include <ebisu_mem.h>
#include <ebisu_str.h>
#include <stdlib.h>
#include <string.h>

/* A buffer of 0 bytes would fail every request with KII_ERR_TOO_LARGE_DATA. */
#if KII_REQ_HEADER_BUFF_SIZE < 1
#error "KII_REQ_HEADER_BUFF_SIZE must be 1 or more"
#endif

const char _APP_KEY_HEADER[] = "X-Kii-Appkey: k";
const char _CONTENT_LENGTH_ZERO[] = "Content-Length: 0";

size_t _cb_write_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    if (kii->_rw_buff_written == 0) {
        // A response refusing the session's token has the request sent again,
        // so it may have to be read past the request's body.
        kii->_resp_offset = _kii_session_resp_offset(kii);
        memset(kii->_rw_buff + kii->_resp_offset, '\0',
                kii->_rw_buff_size - kii->_resp_offset);
    }
    if (kii->_rw_buff_size - kii->_resp_offset - kii->_rw_buff_written < size) {
        // No room after the request's body, which is given up.
        _kii_resp_to_start(kii);
    }
    size_t remain = kii->_rw_buff_size - kii->_resp_offset - kii->_rw_buff_written;
    size_t req_size = size;
    // One byte is kept for a NUL after the body, which the buffer, zeroed
    // above, then has. A safety net, and no part of the API: the body is
    // kii_get_resp_body_length() bytes, and nothing says it is a string. But
    // whoever reads it as one all the same stops within the buffer.
    if (remain <= req_size)
    {
        // Insufficient buffer size.
        return 0;
    }
    memcpy(kii->_rw_buff + kii->_resp_offset + kii->_rw_buff_written, buffer, req_size);
    kii->_rw_buff_written += req_size;
    return req_size;
}

size_t _cb_read_buff(char *buffer, size_t size, void *userdata)
{
    kii_t *kii = (kii_t *)userdata;
    size_t remain = kii->_rw_buff_req_size - kii->_rw_buff_read;
    if (remain <= 0)
    {
        return 0;
    }
    size_t to_read = (size > remain) ? (remain) : (size);
    memcpy(buffer, kii->_rw_buff + kii->_rw_buff_read, to_read);
    kii->_rw_buff_read += to_read;
    return to_read;
}

size_t _cb_write_header(char *buffer, size_t size, void *userdata)
{
    kii_t* kii = (kii_t*)userdata;
    // A response's status line, which khc gives first and has checked: no
    // header starts so, a name having no '/'. kii_get_etag() gives the last
    // response's ETag until here, so that a request that got no response can
    // be tried again with it.
    if (size >= 7 && memcmp(buffer, "HTTP/1.", 7) == 0) {
        kii->_etag[0] = '\0';
        kii->_etag_lost = false;
    }
    if (_parse_etag(buffer, size, kii->_etag, KII_ETAG_MAX_SIZE) == -3) {
        kii->_etag_lost = true;
    }
    _kii_read_resp_header(kii, buffer, size);
    return size;
}

void _kii_clear_resp_headers_but_etag(kii_t* kii)
{
    kii->_resp_kii_error = false;
    kii->_resp_step_count = -1;
    kii->_resp_environment_version = -1;
}

void _kii_clear_resp_headers(kii_t* kii)
{
    kii->_etag[0] = '\0';
    kii->_etag_lost = false;
    _kii_clear_resp_headers_but_etag(kii);
}

/* A count, as X-Step-Count and X-Environment-Version give them: digits only,
 * or -1. */
static int64_t _header_count(ebisu_span_t value)
{
    uint64_t count = 0;
    if (ebisu_parse_uint(value.ptr, value.len, INT64_MAX, &count) != EBISU_PARSE_OK) {
        return -1;
    }
    return (int64_t)count;
}

/* Whether a media type is application/vnd.kii.<name>+json, which Kii gives
 * its errors, as application/vnd.kii.ServerCodeVersionNotFoundException+json:
 * types and subtypes are compared without case, and parameters ignored. */
static bool _is_kii_error_type(ebisu_span_t value)
{
    static const char prefix[] = "application/vnd.kii.";
    static const char suffix[] = "+json";
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t suffix_len = sizeof(suffix) - 1;
    ebisu_span_t type;
    if (!ebisu_span_next(&value, ';', &type)) {
        return false;
    }
    type = ebisu_span_trim(type);
    if (type.len <= prefix_len + suffix_len) {
        return false;
    }
    ebisu_span_t start = { type.ptr, prefix_len };
    ebisu_span_t end = { type.ptr + type.len - suffix_len, suffix_len };
    return ebisu_span_eq_nocase(start, prefix) && ebisu_span_eq_nocase(end, suffix);
}

void _kii_read_resp_header(kii_t* kii, const char* header, size_t header_len)
{
    ebisu_span_t rest = { header, header_len };
    ebisu_span_t name;
    // The status line, which khc gives first, has no colon, or none after a
    // name these are.
    if (!ebisu_span_next(&rest, ':', &name) || rest.ptr == NULL) {
        return;
    }
    ebisu_span_t value = ebisu_span_trim(rest);
    if (ebisu_span_eq_nocase(name, "Content-Type")) {
        kii->_resp_kii_error = _is_kii_error_type(value);
    } else if (ebisu_span_eq_nocase(name, "X-Step-Count")) {
        kii->_resp_step_count = _header_count(value);
    } else if (ebisu_span_eq_nocase(name, "X-Environment-Version")) {
        kii->_resp_environment_version = _header_count(value);
    }
}

bool _kii_etag_unknown(kii_t* kii)
{
    return kii->_etag_lost ||
        (kii->_etag[0] == '\0' && khc_get_resp_header_skipped(&kii->_khc));
}

void kii_init(kii_t* kii)
{
    memset(kii, 0x00, sizeof(kii_t));
    khc_init(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    khc_set_cb_header(&kii->_khc, _cb_write_header, kii);
    _kii_clear_resp_headers(kii);
    kii->_cb_task_continue = NULL;
    kii->_task_continue_data = NULL;
    kii->_cb_task_exit = NULL;
    kii->_task_exit_data = NULL;
    kii->_insecure_mqtt = false;
    kii->_use_m_0_header = false;
    kii->_mqtt_qos = KII_MQTT_QOS_0;
    kii->_mqtt_clean_session = true;
}

/* Copies src into a fixed-size field, truncating rather than overflowing, and
 * always terminating.
 *
 * These fields used to be filled with
 * strncpy(kii->_app_id, app_id, sizeof(kii->_app_id)). strncpy writes no
 * terminator when the source is at least as long as the count, so an app id or
 * host of 128 characters or more left the field unterminated, and every later
 * strlen or strcat over it ran off the end of the struct field.
 *
 * khc_set_host() rejects over-long input with KHC_ERR_TOO_LARGE_DATA instead,
 * which is the better contract, but these two are void and part of the public
 * API. */
static void _kii_copy_bounded(char* dest, size_t dest_size, const char* src)
{
    /* Reads at most dest_size - 1 bytes of src, since strncpy stops at the
     * source's terminator. Measuring src with strlen first would instead scan
     * all of it, however long it is, to copy at most this many bytes. */
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void kii_set_site(
        kii_t* kii,
        const char* site)
{
    /* What 1.x took for Kii Cloud's public site, which its applications may
     * still pass. */
    const char* host = strcmp(site, "JP") == 0 ? "api-jp.kii.com" : site;
    _kii_copy_bounded(kii->_app_host, sizeof(kii->_app_host), host);
}

void kii_set_app_id(
        kii_t* kii,
        const char* app_id)
{
    _kii_copy_bounded(kii->_app_id, sizeof(kii->_app_id), app_id);
}

void kii_set_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_rw_buff = buff;
    kii->_rw_buff_size = buff_size;
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
}

void kii_set_stream_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_stream_buff = buff;
    kii->_stream_buff_size = buff_size;
    khc_set_stream_buff(&kii->_khc, buff, buff_size);
}

void kii_set_resp_header_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_resp_header_buff = buff;
    kii->_resp_header_buff_size = buff_size;
    khc_set_resp_header_buff(&kii->_khc, buff, buff_size);
}

void kii_set_http_socket(kii_t* kii, ebisu_socket_t* socket) {
    khc_set_socket(&kii->_khc, socket);
}

void kii_set_mqtt_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_mqtt_buffer = buff;
    kii->_mqtt_buffer_size = buff_size;
}

void kii_set_mqtt_qos(kii_t* kii, kii_mqtt_qos_t qos) {
    kii->_mqtt_qos = qos;
}

kii_code_t kii_set_mqtt_client_id(kii_t* kii, const char* client_id) {
    // CONNECT carries it with a length of 16 bits.
    if (client_id != NULL && strlen(client_id) > 65535) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_mqtt_client_id = client_id;
    return KII_ERR_OK;
}

void kii_set_cb_mqtt_credentials(kii_t* kii, KII_CB_MQTT_CREDENTIALS cb, void* userdata) {
    kii->_cb_mqtt_credentials = cb;
    kii->_mqtt_credentials_data = userdata;
}

kii_code_t kii_set_mqtt_will(kii_t* kii, const kii_mqtt_will_t* will) {
    if (will != NULL) {
        if (will->topic == NULL || (will->message == NULL && will->message_length > 0) ||
                (will->qos != KII_MQTT_QOS_0 && will->qos != KII_MQTT_QOS_1)) {
            return KII_ERR_FAIL;
        }
        // CONNECT carries each with a length of 16 bits.
        if (strlen(will->topic) > 65535 || will->message_length > 65535) {
            return KII_ERR_TOO_LARGE_DATA;
        }
    }
    kii->_mqtt_will = will;
    return KII_ERR_OK;
}

void kii_set_mqtt_clean_session(kii_t* kii, bool clean_session) {
    kii->_mqtt_clean_session = clean_session;
}

void kii_set_mqtt_socket(kii_t* kii, ebisu_socket_t* socket) {
    kii->_mqtt_socket = socket;
}

void kii_set_mqtt_to_sock_recv(kii_t* kii, uint32_t to_sock_recv_sec) {
    kii->_mqtt_to_recv_sec = to_sock_recv_sec;
}

kii_code_t kii_set_mqtt_retry_wait_max(kii_t* kii, uint32_t max_sec) {
    // The task keeps it in milliseconds. 0 would have it try again at once.
    if (max_sec == 0 || max_sec > UINT32_MAX / 1000) {
        return KII_ERR_FAIL;
    }
    kii->_mqtt_retry_wait_max_ms = max_sec * 1000;
    return KII_ERR_OK;
}

void kii_set_system(kii_t* kii, ebisu_system_t* system) {
    kii->_system = system;
    // The requests wait for their sockets with it, and are timed by it.
    khc_set_system(&kii->_khc, system);
}

kii_code_t kii_set_request_timeout(kii_t* kii, uint32_t timeout_sec) {
    if (timeout_sec > UINT32_MAX / 1000) {
        return KII_ERR_FAIL;
    }
    khc_set_timeout_ms(&kii->_khc, timeout_sec * 1000);
    return KII_ERR_OK;
}

void kii_set_cb_task_continue(kii_t* kii, KII_CB_TASK_CONTINUE cb, void* userdata) {
    kii->_cb_task_continue = cb;
    kii->_task_continue_data = userdata;
}

void kii_set_cb_task_exit(kii_t* kii, KII_CB_TASK_EXIT cb, void* userdata) {
    kii->_cb_task_exit = cb;
    kii->_task_exit_data = userdata;
}

void kii_set_cb_mqtt_event(kii_t* kii, KII_CB_MQTT_EVENT cb, void* userdata) {
    kii->_cb_mqtt_event = cb;
    kii->_mqtt_event_data = userdata;
}

void kii_set_logger(kii_t* kii, ebisu_logger_t* logger) {
    kii->_logger = logger;
    khc_set_logger(&kii->_khc, logger);
}

void kii_set_cb_mqtt_message(kii_t* kii, KII_CB_MQTT_MESSAGE cb, void* userdata) {
    kii->_cb_mqtt_message = cb;
    kii->_mqtt_message_data = userdata;
}

void kii_set_http_lock(kii_t* kii, ebisu_lock_t* lock) {
    kii->_http_lock = lock;
}

void kii_set_mqtt_lock(kii_t* kii, ebisu_lock_t* lock) {
    kii->_mqtt_lock = lock;
}

void kii_set_req_header_buff(kii_t* kii, char* buff, size_t buff_size) {
    kii->_req_header_buff = buff;
    kii->_req_header_buff_size = (buff != NULL) ? buff_size : 0;
}

void kii_enable_insecure_http(
    kii_t* kii,
    bool enable_insecure_http) {
    if (enable_insecure_http) {
        khc_enable_insecure(&kii->_khc, 1);
    } else {
        khc_enable_insecure(&kii->_khc, 0);
    }
}

void kii_enable_insecure_mqtt(
    kii_t* kii,
    bool enable_insecure_mqtt) {
    kii->_insecure_mqtt = enable_insecure_mqtt;
}

void kii_set_mqtt_version(
    kii_t* kii,
    kii_mqtt_version_t version) {
    kii->_mqtt_version = version;
}

void kii_enable_absolute_target(
    kii_t* kii,
    bool enable_absolute_target) {
    khc_enable_absolute_target(&kii->_khc, enable_absolute_target);
}

const char* kii_get_etag(kii_t* kii) {
    return kii->_etag;
}

ebisu_sock_error_t kii_get_sock_error(kii_t* kii, long* out_detail) {
    return khc_get_sock_error(&kii->_khc, out_detail);
}

uint16_t kii_get_resp_status(kii_t* kii) {
    return khc_get_status_code(&kii->_khc);
}

int64_t kii_get_resp_date(kii_t* kii) {
    return khc_get_resp_date(&kii->_khc);
}

size_t kii_get_resp_body_length(kii_t* kii) {
    return kii->_rw_buff_written;
}

void kii_get_author(kii_t* kii, kii_author_t* out_author) {
    if (kii->_session != NULL) {
        kii_session_get_author(kii->_session, out_author);
    } else {
        *out_author = kii->_author;
    }
}

const char* kii_get_thing_id(kii_t* kii) {
    return _kii_thing_id(kii);
}

void kii_set_use_m_0_header_flag(kii_t* kii, bool flag) {
    kii->_use_m_0_header = flag;
}

void kii_cancel_request_being_prepared(kii_t* kii) {
    _req_headers_free_all(kii);
    _body_from_buff(kii);
}

void _body_from_buff(kii_t* kii) {
    kii->_req_streamed = false;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
}


kii_code_t _convert_code(khc_code_t khc_c) {
    switch(khc_c) {
        case KHC_ERR_OK:
            return KII_ERR_OK;
        case KHC_ERR_SOCK_CONNECT:
            return KII_ERR_SOCK_CONNECT;
        case KHC_ERR_SOCK_CLOSE:
            return KII_ERR_SOCK_CLOSE;
        case KHC_ERR_SOCK_SEND:
            return KII_ERR_SOCK_SEND;
        case KHC_ERR_SOCK_RECV:
            return KII_ERR_SOCK_RECV;
        case KHC_ERR_HEADER_CALLBACK:
            return KII_ERR_HEADER_CALLBACK;
        case KHC_ERR_WRITE_CALLBACK:
            return KII_ERR_WRITE_CALLBACK;
        case KHC_ERR_ALLOCATION:
            return KII_ERR_ALLOCATION;
        case KHC_ERR_TOO_LARGE_DATA:
            return KII_ERR_TOO_LARGE_DATA;
        case KHC_ERR_FAIL:
            return KII_ERR_FAIL;
        // A read callback of the application's gave more than it was asked
        // for, or less than kii_api_call_set_body_reader()'s length. kii's own
        // gives its buffer's body whole.
        case KHC_ERR_READ_CALLBACK:
            return KII_ERR_FAIL;
        case KHC_ERR_TIMEOUT:
            return KII_ERR_TIMEOUT;
        // kii sets no continue callback on its khc.
        case KHC_ERR_CANCELLED:
            return KII_ERR_FAIL;
        case KHC_ERR_INVALID_RESPONSE:
            return KII_ERR_INVALID_RESPONSE;
    }
    return KII_ERR_FAIL;
}

bool _kii_field_too_long(kii_t* kii, const jkii_field_t* field, const char* what,
        const char* size_name)
{
    if (field->result != JKII_FIELD_ERR_COPY) {
        return false;
    }
    (void)what;
    (void)size_name;
    _kii_log(kii, EBISU_LOG_ERROR,
            "the server's %s, of %lu bytes, is longer than the %lu %s gives it room for.",
            what, (unsigned long)(field->end - field->start),
            (unsigned long)(field->field_copy_buff_size - 1), size_name);
    return true;
}

bool _kii_field_fits(kii_t* kii, const jkii_field_t* field, size_t size, const char* what,
        const char* size_name)
{
    /* jkii copies a string as it is in the response, so this is the copy's
     * own check. */
    if (field->result != JKII_FIELD_ERR_OK || field->end - field->start < size) {
        return true;
    }
    (void)what;
    (void)size_name;
    _kii_log(kii, EBISU_LOG_ERROR,
            "the server's %s, of %lu bytes, is longer than the %lu %s gives it room for.",
            what, (unsigned long)(field->end - field->start), (unsigned long)(size - 1),
            size_name);
    return false;
}

void _reset_buff(kii_t* kii) {
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_resp_offset = 0;
    kii->_rw_buff_req_size = 0;
    kii->_rw_buff[0] = '\0';
    /* The last response's ETag stays until another response comes, in
     * _cb_write_header(): the application gives it to this call, straight
     * from kii_get_etag(), for its If-Match. */
    _kii_clear_resp_headers_but_etag(kii);
}

/* khc treats its stream and response header buffers as per-request, so
 * khc_reset_except_cb() clears them along with the rest of the request state.
 * kii's setters are per kii_t, and every API resets khc before its request,
 * so the buffers have to be handed back to khc each time. Resetting khc_t
 * directly instead left them NULL, and khc allocated its defaults for every
 * request. */
void _reset_khc(kii_t* kii) {
    khc_reset_except_cb(&kii->_khc);
    khc_set_stream_buff(&kii->_khc, kii->_stream_buff, kii->_stream_buff_size);
    khc_set_resp_header_buff(&kii->_khc, kii->_resp_header_buff, kii->_resp_header_buff_size);
    /* Nothing of a call before goes with the request: the headers of one
     * started with kii_api_call_start() and never run, nor the reader of one
     * cancelled or refused after kii_api_call_set_body_reader(). */
    _req_headers_free_all(kii);
    _body_from_buff(kii);
}

void _kii_clear_buffers(kii_t* kii) {
    if (kii->_rw_buff != NULL) {
        // A response refusing the request holds none, and says why: it stays,
        // and what follows it, the rest of the request, goes. One accepting it
        // goes too, as it may carry a token, and with it the body's length.
        uint16_t status = khc_get_status_code(&kii->_khc);
        size_t keep = 0;
        if (status < 200 || 300 <= status) {
            keep = kii->_resp_offset + kii->_rw_buff_written;
            if (keep > kii->_rw_buff_size) {
                keep = kii->_rw_buff_size;
            }
        } else {
            kii->_rw_buff_written = 0;
        }
        ebisu_secure_zero(kii->_rw_buff + keep, kii->_rw_buff_size - keep);
    }
    if (kii->_stream_buff != NULL) {
        ebisu_secure_zero(kii->_stream_buff, kii->_stream_buff_size);
    }
}

kii_code_t _req_headers_open(kii_t* kii)
{
    if (kii->_req_headers != NULL) {
        return KII_ERR_OK;
    }
    if (kii->_req_header_buff != NULL) {
        kii->_req_headers = kii->_req_header_buff;
        kii->_req_headers_size = kii->_req_header_buff_size;
        kii->_req_headers_allocated = false;
    } else {
        char* buff = malloc(KII_REQ_HEADER_BUFF_SIZE);
        if (buff == NULL) {
            return KII_ERR_ALLOCATION;
        }
        kii->_req_headers = buff;
        kii->_req_headers_size = KII_REQ_HEADER_BUFF_SIZE;
        kii->_req_headers_allocated = true;
    }
    kii->_req_headers_len = 0;
    return KII_ERR_OK;
}

void _req_headers_free_all(kii_t* kii) {
    if (kii->_req_headers != NULL) {
        /* Whole: an Authorization header carries a token, and a header that
         * didn't fit left part of itself after the others. */
        ebisu_secure_zero(kii->_req_headers, kii->_req_headers_size);
        if (kii->_req_headers_allocated) {
            free(kii->_req_headers);
        }
    }
    kii->_req_headers = NULL;
    kii->_req_headers_size = 0;
    kii->_req_headers_len = 0;
    kii->_req_headers_allocated = false;
    /* khc would otherwise keep pointing at them. */
    khc_set_req_headers(&kii->_khc, NULL, 0);
}

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len) {
    const char etag_lower[] = "etag";
    const char etag_upper[] = "ETAG";
    size_t key_len = strlen(etag_lower);
    int state = 0;
    int j = 0;
    for (int i = 0; i < header_len; ++i) {
        char c = header[i];

        if (state == 0) {
            if (c == etag_lower[i] || c == etag_upper[i]) {
                if (i == key_len - 1) {
                    state = 1;
                }
                continue;
            } else {
                // Not Etag.
                return -1;
            }
        } else if (state == 1) { // Skip WP before :
            if (c == ' ' || c == '\t') {
                continue;
            } else if ( c == ':') {
                state = 2;
                continue;
            } else {
                // Inalid Format.
                return -2;
            }
        } else if (state == 2) { // Skip WP after :
            if (c == ' ' || c == '\t') {
                continue;
            } else {
                state = 3;
                buff[0] = c;
                j++;
                continue;
            }
        } else if (state == 3) { // Extract value
            if (c == ' ' || c == '\t' || c == '\r') {
                break;
            } else {
                if (j < buff_len - 1) {
                    buff[j] = c;
                    ++j;
                    continue;
                } else {
                    // Too long for the buffer: none, rather than part of it.
                    buff[0] = '\0';
                    return -3;
                }
            }
        }
    }
    buff[j] = '\0';
    return j;
}
