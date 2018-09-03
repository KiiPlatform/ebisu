#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_json_utils.h"

static kii_code_t _thing_authentication(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password
        )
{
    khc_set_host(&kii->_khc, kii->_app_host);
    // /api/apps/{appid}/oauth2/token
    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "/api/apps/%s/oauth2/token", kii->_app_id);
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
    khc_set_method(&kii->_khc, "POST");

    // Request body.
    char esc_vid[strlen(vendor_thing_id) * 2];
    char esc_pass[strlen(password) * 2];
    kii_escape_str(vendor_thing_id, esc_vid, sizeof(esc_vid) * sizeof(char));
    kii_escape_str(password, esc_pass, sizeof(esc_vid) * sizeof(char));

    int content_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "{\"username\":\"VENDOR_THING_ID:%s\", \"password\":\"%s\", \"grant_type\":\"password\"}",
        esc_vid, esc_pass);
    if (content_len >= 256) {
        return KII_ERR_TOO_LARGE_DATA;
    }

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);

    char ct[] = "Content-Type: application/vnd.kii.OauthTokenRequest+json";
    headers = khc_slist_append(headers, ct, strlen(ct));

    char appkey[] = "X-Kii-Appkey: k";
    headers = khc_slist_append(headers, appkey, strlen(appkey));
    char cl_h[128];
    int cl_h_len = snprintf(cl_h, 128, "Content-Length: %d", content_len);
    if (cl_h_len >= 128) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, cl_h, cl_h_len);
    khc_set_req_headers(&kii->_khc, headers);

    _kii_set_content_length(kii, content_len);

    khc_code code = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);

    return _convert_code(code);
}

static khc_code _register_thing_with_id(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

kii_code_t kii_thing_authenticate(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    kii_code_t ret = KII_ERR_FAIL;

    ret = _thing_authentication(kii, vendor_thing_id, password);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_size;
    if (buff == NULL) {
        ret = KII_ERR_FAIL;
        goto exit;
    }
    kii_json_field_t fields[3];
    kii_json_parse_result_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "id";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.author_id;
    fields[0].field_copy_buff_size = sizeof(kii->_author.author_id) /
            sizeof(kii->_author.author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.access_token;
    fields[1].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = KII_ERR_PARSE_JSON;
        goto exit;
    }

    ret = KII_ERR_OK;

exit:
    return ret;
}

int kii_thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    int ret = -1;

    khc_code khc_err = _register_thing_with_id(kii, vendor_thing_id,
            password, thing_type);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    /* parse response */
    // TODO: get buffer and its length.
    char* buff = NULL;
    size_t buff_size = 0;
    if (buff == NULL) {
        goto exit;
    }

    kii_json_field_t fields[3];
    kii_json_parse_result_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "_accessToken";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[1].name = "_thingID";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->_author.author_id) /
            sizeof(kii->_author.author_id[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
