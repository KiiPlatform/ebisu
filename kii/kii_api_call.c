#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include <string.h>

kii_code_t kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        kii_bool_t set_authentication_header)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, http_method);
    khc_set_path(&kii->_khc, resource_path);

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
    if (set_authentication_header == KII_TRUE) {
        res = _set_auth_header(kii);
        if (res != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return res;
        }
    }
    return res;
}

kii_code_t kii_api_call_append_body(
        kii_t* kii,
        const char* chunk,
        size_t chunk_size)
{
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

kii_code_t kii_api_call_append_header(kii_t* kii, const char* key, const char* value)
{
    size_t key_len = strlen(key);
    size_t val_len = strlen(value);
    size_t buff_size = key_len + val_len + 3;
    char buff[buff_size];

    int header_len = snprintf(buff, buff_size, "%s: %s", key, value);
    if (header_len >= buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_slist* list = khc_slist_append(kii->_req_headers, buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;

    return KII_ERR_OK;
}

kii_code_t kii_api_call_run(kii_t* kii)
{
    kii_code_t res = _set_content_length(kii, kii->_rw_buff_req_size);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}