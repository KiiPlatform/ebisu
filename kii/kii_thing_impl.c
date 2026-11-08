#include "kii_thing_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "jkii_utils.h"
#include <ebisu_mem.h>
#include <ebisu_str.h>

kii_code_t _thing_auth(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");
    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&path, "/api/apps/%s/oauth2/token", kii->_app_id);
    if (!ebisu_strbuf_ok(&path)) {
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
    int content_len = _kii_body_append(kii, 0,
            "{\"username\":\"VENDOR_THING_ID:", vendor_thing_id, "");
    content_len = _kii_body_append(kii, content_len,
            "\", \"password\":\"", password, "\", \"grant_type\":\"password\"}");
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    return _req_send(kii);
}

kii_code_t _thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&path, "/api/apps/%s/things", kii->_app_id);
    if (!ebisu_strbuf_ok(&path)) {
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

    return _req_send(kii);
}

kii_code_t _thing_refresh(
        kii_t* kii,
        const char* refresh_token)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");
    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&path, "/api/apps/%s/oauth2/token", kii->_app_id);
    if (!ebisu_strbuf_ok(&path)) {
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
    // Request body. Tokens are printable, which escaping at most doubles.
    char esc_token[KII_REFRESH_TOKEN_MAX_SIZE * 2];
    if (jkii_escape_str(refresh_token, esc_token, sizeof(esc_token)) < 0) {
        ebisu_secure_zero(esc_token, sizeof(esc_token));
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    ebisu_strbuf_t body;
    ebisu_strbuf_init(&body, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&body,
            "{\"grant_type\":\"refresh_token\", \"refresh_token\":\"%s\"}",
            esc_token);
    ebisu_secure_zero(esc_token, sizeof(esc_token));
    if (!ebisu_strbuf_ok(&body)) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = body.len;

    return _req_send(kii);
}
