#include <ebisu_str.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"

// With as_thing, the request carries the token of the thing kii acts as, and
// otherwise access_token, or none if it is NULL.
static kii_code_t _execute_server_code(
        kii_t* kii,
        const char* version,
        const char* endpoint_name,
        const char* params,
        bool as_thing,
        const char* access_token)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    ebisu_strbuf_t path;
    ebisu_strbuf_init(&path, kii->_rw_buff, kii->_rw_buff_size);
    ebisu_strbuf_appendf(&path, "/api/apps/%s/server-code/versions/%s/%s",
            kii->_app_id, version, endpoint_name);

    // The version and the endpoint's name come from the application. Too long
    // for khc, the request would go without its path.
    if (!ebisu_strbuf_ok(&path) ||
            khc_set_path(&kii->_khc, kii->_rw_buff) != KHC_ERR_OK) {
        return KII_ERR_TOO_LARGE_DATA;
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
    if (as_thing) {
        res = _set_auth_header(kii);
    } else if (access_token != NULL) {
        res = _set_auth_bearer_token(kii, access_token);
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
        bool as_thing,
        const char* access_token)
{
    _reset_khc(kii);
    _reset_buff(kii);


    kii_code_t res = _execute_server_code(kii, version, endpoint_name, params,
            as_thing, access_token);
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
    return _run_server_code(kii, version, endpoint_name, params, true, NULL);
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
            endpoint_name, params, false, access_token);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

