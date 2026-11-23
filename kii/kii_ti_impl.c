#include "kii_ti_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "jkii_wrapper.h"
#include <string.h>

kii_code_t _get_anonymous_token(
        kii_t* kii,
        _kii_token_t* out_token)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);

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
    ret = _set_content_type(kii, "application/json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        ret = _set_m_0_header(kii);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }
    // Request body.
    int content_len = snprintf(
            kii->_rw_buff,
            kii->_rw_buff_size,
            "{\"grant_type\":\"client_credentials\",\"client_id\":\"%s\",\"client_secret\":\"dummy\"}",
            kii->_app_id);
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    ret = _convert_code(code);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    jkii_field_t fields[2];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "access_token";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_token->token;
    fields[0].field_copy_buff_size = sizeof(out_token->token) /
        sizeof(out_token->token[0]);
    fields[1].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }

    return KII_ERR_OK;
}

kii_code_t _onboard(
        kii_t* kii,
        const char* token,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/onboardings", kii->_app_id);
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
    ret = _set_auth_bearer_token(kii, token);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, "application/vnd.kii.OnboardingWithVendorThingIDByThing+json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        ret = _set_m_0_header(kii);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }

    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"vendorThingID\":\"", vendor_thing_id, "");
    content_len = _kii_body_append(kii, content_len,
            "\",\"thingPassword\":\"", password, "\"");
    if (thing_type != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"thingType\":\"", thing_type, "\"");
    }
    if (firmware_version != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"firmwareVersion\":\"", firmware_version, "\"");
    }
    if (layout_position != NULL) {
        content_len = _kii_body_append(kii, content_len,
                ",\"layoutPosition\":\"", layout_position, "\"");
    }
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    if (thing_properties != NULL) {
        // A JSON object, not a string: embedded as given, unlike the values
        // above, which are escaped to go between quotes.
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"thingProperties\":%s",
                thing_properties);
        if (content_len >= kii->_rw_buff_size) {
            _req_headers_free_all(kii);
            return KII_ERR_TOO_LARGE_DATA;
        }
    }

    content_len += snprintf(
            &kii->_rw_buff[content_len],
            kii->_rw_buff_size - content_len,
            "}");
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    ret = _convert_code(code);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    kii_code_t res = _kii_read_author(kii, buff, buff_size, "thingID", "accessToken");
    if (res != KII_ERR_OK) {
        return res;
    }

    return KII_ERR_OK;
}

kii_code_t _put_firmware_version(
        kii_t* kii,
        const char* firmware_version)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/things/%s/firmware-version", kii->_app_id, kii->_author.author_id);
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
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, "application/vnd.kii.ThingFirmwareVersionUpdateRequest+json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        ret = _set_m_0_header(kii);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }

    // Request body.
    int content_len = _kii_body_append(kii, 0,
            "{\"firmwareVersion\":\"", firmware_version, "\"}");
    if (content_len < 0) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    ret = _convert_code(code);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    return KII_ERR_OK;
}

kii_code_t _get_firmware_version(
        kii_t* kii,
        kii_ti_firmware_version_t* out_version)
{
    kii_code_t ret = KII_ERR_FAIL;
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);

    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "GET");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/things/%s/firmware-version", kii->_app_id, kii->_author.author_id);
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
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        ret = _set_m_0_header(kii);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = _kii_perform(kii);
    _req_headers_free_all(kii);

    ret = _convert_code(code);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        return KII_ERR_FAIL;
    }

    jkii_field_t fields[2];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "firmwareVersion";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_version->firmware_version;
    fields[0].field_copy_buff_size = sizeof(out_version->firmware_version) /
        sizeof(out_version->firmware_version[0]);
    fields[1].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK) {
        return KII_ERR_PARSE_JSON;
    }

    return KII_ERR_OK;
}

kii_code_t _upload_state(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* last_path_segment,
        const char* method,
        const char* content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    kii_code_t ret = KII_ERR_FAIL;
    khc_reset_except_cb(&kii->_khc);
    _reset_buff(kii);

    // The application's host fits, set by kii_set_site(). The caller's may
    // not.
    khc_code host_res = khc_set_host(&kii->_khc, opt_normalizer_host != NULL ?
                opt_normalizer_host : kii->_app_host);
    if (host_res != KHC_ERR_OK) {
        return _convert_code(host_res);
    }
    khc_set_method(&kii->_khc, method);

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/targets/thing:%s/%s", kii->_app_id, kii->_author.author_id, last_path_segment);
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
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_content_type(kii, content_type);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    if (opt_content_encoding != NULL) {
        ret = _set_content_encoding(kii, opt_content_encoding);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }
    if (kii->_use_m_0_header == KII_TRUE) {
        ret = _set_m_0_header(kii);
        if (ret != KII_ERR_OK) {
            _req_headers_free_all(kii);
            return ret;
        }
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_set_cb_read(&kii->_khc, state_read_cb, state_read_cb_data);
    khc_code code = _kii_perform(kii);
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    _req_headers_free_all(kii);

    ret = _convert_code(code);
    if (ret != KII_ERR_OK) {
        return ret;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        return KII_ERR_RESP_STATUS;
    }

    return KII_ERR_OK;
}

