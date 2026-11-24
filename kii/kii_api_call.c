#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "kii_session_impl.h"
#include "ebisu_version.h"
#include "jkii_utils.h"
#include <ebisu_str.h>
#include <string.h>

/* The SDK and its version, as git describe gives it for the tree built. */
#define _SDK_INFO "sn=te;sv=" EBISU_VERSION_FULL

/* Copies str to the buffer at len, and returns the new length, or -1. */
static int _body_copy(kii_t* kii, int len, const char* str)
{
    size_t room = kii->_rw_buff_size - len;
    size_t str_len = strlen(str);
    if (str_len >= room) {
        return -1;
    }
    memcpy(&kii->_rw_buff[len], str, str_len + 1);
    return len + (int)str_len;
}

int _kii_body_append(
        kii_t* kii,
        int len,
        const char* before,
        const char* value,
        const char* after)
{
    if (len < 0) {
        return -1;
    }
    len = _body_copy(kii, len, before);
    if (len < 0) {
        return -1;
    }
    // Into the buffer itself: a value has to fit there anyway, and the
    // application decides how long it is, so it can't size an array on the
    // stack of the task making the request.
    int escaped = jkii_escape_str(value, &kii->_rw_buff[len], kii->_rw_buff_size - len);
    if (escaped < 0) {
        return -1;
    }
    return _body_copy(kii, len + escaped, after);
}

/* Takes the body from the buffer again, after one set by
 * kii_api_call_set_body_reader(). */
static void _body_from_buff(kii_t* kii)
{
    kii->_req_streamed = false;
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
}

static void _resource_cleanup(kii_t* kii)
{
    _reset_buff(kii);
    _req_headers_free_all(kii);

    _reset_khc(kii);
    _body_from_buff(kii);
}

kii_code_t kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        bool set_authentication_header)
{
    _resource_cleanup(kii);
    
    khc_set_host(&kii->_khc, kii->_app_host);
    // Both come from the application. Too long for khc, or with what would
    // break the request line, the request would go without them.
    khc_code_t k_res = khc_set_method(&kii->_khc, http_method);
    if (k_res == KHC_ERR_OK) {
        k_res = khc_set_path(&kii->_khc, resource_path);
    }
    if (k_res != KHC_ERR_OK) {
        return _convert_code(k_res);
    }

    kii_code_t res = _set_app_id_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_app_key_header(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    if (content_type != NULL && strlen(content_type) > 0) {
        res = _set_content_type(kii, content_type);
        if (res != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return res;
        }
    }
    if (set_authentication_header) {
        res = _set_auth_header(kii);
        if (res != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return res;
        }
    }
    return res;
}

kii_code_t kii_api_call_set_no_body(kii_t* kii)
{
    _reset_buff(kii);
    return KII_ERR_OK;
}

kii_code_t kii_api_call_set_body_reader(
        kii_t* kii,
        KII_CB_READ read_cb,
        void* userdata,
        size_t length)
{
    // One body or the other: the buffer's would not be sent.
    if (read_cb == NULL || kii->_rw_buff_req_size > 0) {
        _req_headers_free_all(kii);
        return KII_ERR_FAIL;
    }
    khc_set_cb_read(&kii->_khc, read_cb, userdata);
    khc_set_req_body_length(&kii->_khc, length);
    kii->_req_streamed = true;
    return KII_ERR_OK;
}

kii_code_t kii_api_call_append_body(
        kii_t* kii,
        const char* chunk,
        size_t chunk_size)
{
    // After kii_api_call_set_body_reader(), it would not be sent.
    if (kii->_req_streamed) {
        _req_headers_free_all(kii);
        return KII_ERR_FAIL;
    }
    size_t total = kii->_rw_buff_req_size + chunk_size;
    if (total + 1 > kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    char* curr_pos = kii->_rw_buff + kii->_rw_buff_req_size;
    memcpy(curr_pos, chunk, chunk_size);
    kii->_rw_buff_req_size = total;
    kii->_rw_buff[total] = '\0';
    return KII_ERR_OK;
}

kii_code_t kii_api_call_append_header_or_default(
        kii_t* kii,
        const char* key,
        const char* value,
        const char* default_value)
{
    if (value != NULL && strlen(value) > 0) {
        return kii_api_call_append_header(kii, key, value);
    } else {
        return kii_api_call_append_header(kii, key, default_value);
    }
}

kii_code_t kii_api_call_append_header(kii_t* kii, const char* key, const char* value)
{
    // A line end in either would make it two headers, or the end of them.
    // khc_perform() would refuse it; refused here, the call fails where the
    // header was given.
    if (strpbrk(key, "\r\n") != NULL || strpbrk(value, "\r\n") != NULL) {
        _req_headers_free_all(kii);
        return KII_ERR_FAIL;
    }
    kii_code_t res = _req_headers_open(kii);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    // After the headers before it, as khc sends them.
    ebisu_strbuf_t header;
    ebisu_strbuf_init(&header, &kii->_req_headers[kii->_req_headers_len],
            kii->_req_headers_size - kii->_req_headers_len);
    ebisu_strbuf_appendf(&header, "%s: %s\r\n", key, value);
    if (!ebisu_strbuf_ok(&header)) {
        // Its name only: the value may be a token.
        _kii_log(kii, EBISU_LOG_WARNING,
                "the %s header doesn't fit in the request header buffer of %lu bytes",
                key, (unsigned long)kii->_req_headers_size);
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers_len += header.len;
    return KII_ERR_OK;
}

kii_code_t _req_send(kii_t* kii)
{
    /* Here, with the request made, rather than as the call starts: the ETag
     * of the last response is what the application gives this one for its
     * If-Match, straight from kii_get_etag(). */
    _kii_clear_resp_headers(kii);
    kii_code_t res = kii_api_call_append_header(kii, "X-Kii-SDK", _SDK_INFO);
    if (res == KII_ERR_OK && kii->_use_m_0_header) {
        res = _set_m_0_header(kii);
    }
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    khc_set_req_headers(&kii->_khc, kii->_req_headers, kii->_req_headers_len);
    // A body kii holds whole in its buffer goes with its length, rather than
    // chunked, which some intermediaries handle poorly. One the application
    // streams through a callback of its own is still chunked.
    if (kii->_khc._cb_read == _cb_read_buff) {
        khc_set_req_body_length(&kii->_khc, kii->_rw_buff_req_size);
    }
    khc_code_t code = _kii_perform(kii);
    _kii_clock_record(kii);
    // A response without a body wrote nothing over the request's, which is
    // done with. The same safety net as in _cb_write_buff(): whoever reads the
    // buffer as a string finds an empty one, not the request.
    if (kii->_khc._cb_write == _cb_write_buff && kii->_rw_buff_written == 0 &&
            kii->_rw_buff != NULL && kii->_rw_buff_size > 0) {
        kii->_rw_buff[0] = '\0';
    }
    _req_headers_free_all(kii);

    // kii's own write callback fails only when the response doesn't fit in the
    // buffer. One the application gave, to download an object, fails for its
    // own reasons.
    if (code == KHC_ERR_WRITE_CALLBACK && kii->_khc._cb_write == _cb_write_buff) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return _convert_code(code);
}

kii_code_t kii_api_call_run(kii_t* kii)
{
    kii_code_t res = _req_send(kii);
    // A body set by kii_api_call_set_body_reader() was this call's.
    _body_from_buff(kii);
    return res;
}
