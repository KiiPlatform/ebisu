#include "kii_thing_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "kii_json_utils.h"
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

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
    // Request body.
    char esc_vid[strlen(vendor_thing_id) * 2 + 1];
    char esc_pass[strlen(password) * 2 + 1];
    kii_escape_str(vendor_thing_id, esc_vid, sizeof(esc_vid));
    kii_escape_str(password, esc_pass, sizeof(esc_vid));

    int content_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "{\"username\":\"VENDOR_THING_ID:%s\", \"password\":\"%s\", \"grant_type\":\"password\"}",
        esc_vid, esc_pass);
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    res = _set_content_length(kii, content_len);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

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

    // Request body.
    char esc_vid[strlen(vendor_thing_id) * 2 + 1];
    char esc_pass[strlen(password) * 2 + 1];
    char esc_type[strlen(thing_type) * 2 + 1];
    kii_escape_str(vendor_thing_id, esc_vid, sizeof(esc_vid));
    kii_escape_str(password, esc_pass, sizeof(esc_vid));
    kii_escape_str(thing_type, esc_type, sizeof(esc_type));

    int content_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "{\"_vendorThingID\":\"%s\", \"_thingType\":\"%s\", \"_password\":\"%s\"}",
        esc_vid, esc_type, esc_pass);
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    res = _set_content_length(kii, content_len);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}