#include "kii.h"
#include "kii_impl.h"
#include "kii_session_impl.h"
#include <string.h>

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
    if (remain < req_size)
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
    char* etag_buff = ((kii_t*)userdata)->_etag;
    _parse_etag(buffer, size, etag_buff, KII_ETAG_MAX_SIZE);
    return size;
}

void kii_init(kii_t* kii)
{
    memset(kii, 0x00, sizeof(kii_t));
    khc_init(&kii->_khc);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    khc_set_cb_header(&kii->_khc, _cb_write_header, kii);
    kii->_etag[0] = '\0';
    kii->_cb_slist_alloc = khc_cb_slist_alloc;
    kii->_cb_slist_free = khc_cb_slist_free;
    kii->_slist_alloc_data = NULL;
    kii->_slist_free_data = NULL;
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
}

void kii_set_cb_task_continue(kii_t* kii, KII_CB_TASK_CONTINUE cb, void* userdata) {
    kii->_cb_task_continue = cb;
    kii->_task_continue_data = userdata;
}

void kii_set_cb_task_exit(kii_t* kii, KII_CB_TASK_EXIT cb, void* userdata) {
    kii->_cb_task_exit = cb;
    kii->_task_exit_data = userdata;
}

void kii_set_json_parser_resource(kii_t* kii, jkii_resource_t* resource) {
    kii->_json_resource = resource;
}

void kii_set_cb_json_parser_resource(
    kii_t* kii,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free)
{
    kii->_cb_json_alloc = cb_alloc;
    kii->_cb_json_free = cb_free;
}

void kii_set_cb_slist_resource(
        kii_t* kii,
        KHC_CB_SLIST_ALLOC cb_alloc,
        KHC_CB_SLIST_FREE cb_free,
        void* cb_alloc_data,
        void* cb_free_data) {
    kii->_cb_slist_alloc = cb_alloc;
    kii->_cb_slist_free = cb_free;
    kii->_slist_alloc_data = cb_alloc_data;
    kii->_slist_free_data = cb_free_data;
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

void kii_set_use_m_0_header_flag(kii_t* kii, bool flag) {
    kii->_use_m_0_header = flag;
}

void kii_cancel_request_being_prepared(kii_t* kii) {
    _req_headers_free_all(kii);
}


kii_code_t _convert_code(khc_code khc_c) {
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
        // Only a read callback of the application's can give more than it was
        // asked for. The body length kii sets is that of its buffer, which its
        // own read callback gives whole.
        case KHC_ERR_READ_CALLBACK:
            return KII_ERR_FAIL;
        // kii sets neither a timeout nor a continue callback on its khc.
        case KHC_ERR_TIMEOUT:
        case KHC_ERR_CANCELLED:
            return KII_ERR_FAIL;
    }
    return KII_ERR_FAIL;
}

void _reset_buff(kii_t* kii) {
    kii->_rw_buff_read = 0;
    kii->_rw_buff_written = 0;
    kii->_resp_offset = 0;
    kii->_rw_buff_req_size = 0;
    kii->_rw_buff[0] = '\0';
    kii->_etag[0] = '\0';
}

/* khc treats its stream and response header buffers as per-request, so
 * khc_reset_except_cb() clears them along with the rest of the request state.
 * kii's setters are per kii_t, and every API resets khc before its request,
 * so the buffers have to be handed back to khc each time. Resetting khc
 * directly instead left them NULL, and khc allocated its defaults for every
 * request. */
void _reset_khc(kii_t* kii) {
    khc_reset_except_cb(&kii->_khc);
    khc_set_stream_buff(&kii->_khc, kii->_stream_buff, kii->_stream_buff_size);
    khc_set_resp_header_buff(&kii->_khc, kii->_resp_header_buff, kii->_resp_header_buff_size);
}

void _req_headers_free_all(kii_t* kii) {
    khc_slist_free_all_using_cb_free(kii->_req_headers, kii->_cb_slist_free, kii->_slist_free_data);
    kii->_req_headers = NULL;
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
                    // Etag too large.
                    return -3;
                }
            }
        }
    }
    buff[j] = '\0';
    return j;
}
