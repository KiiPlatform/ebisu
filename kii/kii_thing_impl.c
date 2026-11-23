#include "kii_thing_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "jkii_wrapper.h"
#include "jkii_utils.h"
#include <string.h>

kii_code_t _thing_auth(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");
    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/api/apps/%s/oauth2/token", kii->_app_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    // khc keeps a path shorter than the buffer may hold one, and refuses
    // one that would break the request line.
    khc_code path_res = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (path_res != KHC_ERR_OK) {
        return _convert_code(path_res);
    }

    // Request headers.
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
    res = _set_content_type(kii, "application/vnd.kii.OauthTokenRequest+json");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        res = _set_m_0_header(kii);
        if (res != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return res;
        }
    }
    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"username\":\"VENDOR_THING_ID:", vendor_thing_id, "");
    content_len = _kii_body_append(kii, content_len,
            "\", \"password\":\"", password, "\", \"grant_type\":\"password\"}");
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/api/apps/%s/things", kii->_app_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    // khc keeps a path shorter than the buffer may hold one, and refuses
    // one that would break the request line.
    khc_code path_res = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (path_res != KHC_ERR_OK) {
        return _convert_code(path_res);
    }

    // Request headers.
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
    res = _set_content_type(kii, "application/vnd.kii.ThingRegistrationAndAuthorizationRequest+json");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        res = _set_m_0_header(kii);
        if (res != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return res;
        }
    }

    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"_vendorThingID\":\"", vendor_thing_id, "");
    content_len = _kii_body_append(kii, content_len,
            "\", \"_thingType\":\"", thing_type, "");
    content_len = _kii_body_append(kii, content_len,
            "\", \"_password\":\"", password, "\"}");
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    return _convert_code(code);
}
