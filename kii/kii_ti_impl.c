#include "kii_ti_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"
#include "jkii_wrapper.h"
#include "jkii_utils.h"
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_content_type(kii, "application/json");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
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
    khc_code code = khc_perform(&kii->_khc);
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

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

    // Request body.
    int content_len = 0;
    char esc_vid[strlen(vendor_thing_id) * 2 + 1];
    char esc_pass[strlen(password) * 2 + 1];
    jkii_escape_str(vendor_thing_id, esc_vid, sizeof(esc_vid));
    jkii_escape_str(password, esc_pass, sizeof(esc_vid));

    content_len += snprintf(
            kii->_rw_buff,
            kii->_rw_buff_size,
            "{\"vendorThingID\":\"%s\",\"thingPassword\":\"%s\"",
            esc_vid, esc_pass);
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    if (thing_type != NULL) {
        char esc[strlen(thing_type) * 2 + 1];
        jkii_escape_str(thing_type, esc, sizeof(esc));
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"thingType\":\"%s\"",
                esc);
        if (content_len >= kii->_rw_buff_size) {
            _req_headers_free_all(kii);
            return KII_ERR_TOO_LARGE_DATA;
        }
    }

    if (firmware_version != NULL) {
        char esc[strlen(firmware_version) * 2 + 1];
        jkii_escape_str(firmware_version, esc, sizeof(esc));
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"firmwareVersion\":\"%s\"",
                esc);
        if (content_len >= kii->_rw_buff_size) {
            _req_headers_free_all(kii);
            return KII_ERR_TOO_LARGE_DATA;
        }
    }

    if (layout_position != NULL) {
        char esc[strlen(layout_position) * 2 + 1];
        jkii_escape_str(layout_position, esc, sizeof(esc));
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"layoutPosition\":\"%s\"",
                esc);
        if (content_len >= kii->_rw_buff_size) {
            _req_headers_free_all(kii);
            return KII_ERR_TOO_LARGE_DATA;
        }
    }

    if (thing_properties != NULL) {
        char esc[strlen(thing_properties) * 2 + 1];
        jkii_escape_str(thing_properties, esc, sizeof(esc));
        content_len += snprintf(
                &kii->_rw_buff[content_len],
                kii->_rw_buff_size - content_len,
                ",\"thingProperties\":\"%s\"",
                esc);
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
    khc_code code = khc_perform(&kii->_khc);
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

    jkii_field_t fields[3];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "accessToken";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
        sizeof(kii->_author.access_token[0]);
    fields[1].name = "thingID";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->_author.author_id) /
        sizeof(kii->_author.author_id[0]);
    fields[2].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK) {
        return KII_ERR_PARSE_JSON;
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

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

    // Request body.
    int content_len = snprintf(
            kii->_rw_buff,
            kii->_rw_buff_size,
            "{\"firmwareVersion\":\"%s\"}",
            firmware_version);
    if (content_len >= kii->_rw_buff_size) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_rw_buff_req_size = content_len;

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
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
    khc_set_path(&kii->_khc, kii->_rw_buff);

    // Request headers.
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
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

    if (opt_normalizer_host != NULL) {
        khc_set_host(&kii->_khc, opt_normalizer_host);
    } else {
        khc_set_host(&kii->_khc, kii->_app_host);
    }
    khc_set_method(&kii->_khc, method);

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/thing-if/apps/%s/targets/thing:%s/%s", kii->_app_id, kii->_author.author_id, last_path_segment);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

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
        _set_content_encoding(kii, opt_content_encoding);
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_set_cb_read(&kii->_khc, state_read_cb, state_read_cb_data);
    khc_code code = khc_perform(&kii->_khc);
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

