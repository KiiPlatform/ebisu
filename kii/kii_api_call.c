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
        const void* body_data,
        size_t body_size)
{
    // TODO: implement it.
    return -1;
}

kii_code_t kii_api_call_append_header(kii_t* kii, const char* key, const char* value)
{
    // TODO: implement it.
    return -1;
}

kii_code_t kii_api_call_run(kii_t* kii)
{
    // TODO: implement it.
    return -1;
}