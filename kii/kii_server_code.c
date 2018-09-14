#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_req_impl.h"

static kii_code_t _execute_server_code(
    kii_t* kii,
    const char* endpoint_name,
    const char* params)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    int path_len = snprintf(kii->_rw_buff, kii->_rw_buff_size,
        "/api/apps/%s/server-code/versions/current/%s",
        kii->_app_id, endpoint_name);

    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);

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
    res = _set_auth_header(kii);
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

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t kii_execute_server_code(
    kii_t* kii,
    const char* endpoint_name,
    const char* params)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);


    kii_code_t res = _execute_server_code(kii, endpoint_name, params);
    if (res != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    return res;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

