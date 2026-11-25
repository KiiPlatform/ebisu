#include <ebisu_str.h>
#include <string.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"

// What a request authorizes itself with: the token of the thing kii acts as,
// another token, or none if it is NULL, or the application's own credentials.
typedef enum _server_code_auth_t {
    _SERVER_CODE_AS_THING,
    _SERVER_CODE_WITH_TOKEN,
    _SERVER_CODE_AS_APP
} _server_code_auth_t;

static kii_code_t _execute_server_code(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params,
        _server_code_auth_t auth,
        const char* access_token)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    if (!_kii_is_path_segment(version) || !_kii_is_path_segment(endpoint_name)) {
        return KII_ERR_FAIL;
    }
    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&path, "/api/apps/%s/server-code/versions/%s/%s",
            kii->_app_id, version, endpoint_name);

    // The version and the endpoint's name come from the application. Too long
    // for khc, the request would go without its path.
    if (!ebisu_strbuf_ok(&path)) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_code_t path_res = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (path_res != KHC_ERR_OK) {
        return _convert_code(path_res);
    }

    // Request headers
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
    switch (auth) {
        case _SERVER_CODE_AS_THING:
            res = _set_auth_header(kii);
            break;
        case _SERVER_CODE_AS_APP:
            // After the path, which khc has copied: the header is made in the
            // same buffer.
            res = _set_auth_basic_app(kii);
            break;
        default:
            if (access_token != NULL) {
                res = _set_auth_bearer_token(kii, access_token);
            }
            break;
    }
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    res = _set_content_type(kii, "application/json");
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }
    // Request body
    res = _set_req_body(kii, params);
    if (res != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return res;
    }

    return _req_send(kii);
}

static kii_code_t _run_server_code(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params,
        _server_code_auth_t auth,
        const char* access_token)
{
    _reset_khc(kii);
    _reset_buff(kii);


    kii_code_t res = _execute_server_code(kii, version, endpoint_name, params,
            auth, access_token);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}


kii_code_t kii_execute_server_code_version(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params)
{
    return _run_server_code(kii, version, endpoint_name, params, _SERVER_CODE_AS_THING, NULL);
}

kii_code_t kii_execute_server_code(
        kii_t* kii,
        const char* endpoint_name,
        const char* params)
{
    return kii_execute_server_code_version(kii, "current", endpoint_name, params);
}

kii_code_t kii_execute_server_code_with_token(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params,
        const char* access_token)
{
    return _run_server_code(kii, version != NULL ? version : "current",
            endpoint_name, params, _SERVER_CODE_WITH_TOKEN, access_token);
}

kii_code_t kii_execute_server_code_with_app_auth(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params)
{
    return _run_server_code(kii, version != NULL ? version : "current",
            endpoint_name, params, _SERVER_CODE_AS_APP, NULL);
}
/* Reads errorCode, message and details from the body of an error of Kii's
 * own, if errorCode is a string. */
static void _read_kii_error(
        const char* body,
        size_t body_length,
        kii_server_code_result_t* out_result)
{
    jkii_member_t member;
    if (jkii_find_member(body, body_length, "/errorCode", &member) != JKII_ERR_OK ||
            member.type != JKII_FIELD_TYPE_STRING) {
        return;
    }
    out_result->error_code = member;
    if (jkii_find_member(body, body_length, "/message", &member) == JKII_ERR_OK &&
            member.type == JKII_FIELD_TYPE_STRING) {
        out_result->message = member;
    }
    if (jkii_find_member(body, body_length, "/details", &member) == JKII_ERR_OK) {
        out_result->details = member;
    }
}

kii_code_t kii_get_server_code_result(
        kii_t* kii,
        kii_server_code_result_t* out_result)
{
    memset(out_result, 0, sizeof(*out_result));
    out_result->status = khc_get_status_code(&kii->_khc);
    out_result->step_count = kii->_resp_step_count;
    out_result->environment_version = kii->_resp_environment_version;
    if (kii->_rw_buff != NULL) {
        out_result->body = kii->_rw_buff + kii->_resp_offset;
        out_result->body_length = kii->_rw_buff_written;
    }
    const char* body = out_result->body;
    size_t body_length = out_result->body_length;

    uint16_t status = out_result->status;
    if (status == 0) {
        return KII_ERR_FAIL;
    }
    if (200 <= status && status < 300) {
        if (body == NULL || jkii_find_member(body, body_length,
                    "/returnedValue", &out_result->returned_value) != JKII_ERR_OK) {
            return KII_ERR_PARSE_JSON;
        }
        return KII_ERR_OK;
    }
    if (body != NULL && kii->_resp_kii_error) {
        _read_kii_error(body, body_length, out_result);
    }
    return KII_ERR_RESP_STATUS;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

