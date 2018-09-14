#include <string.h>
#include "kii_object_impl.h"
#include "kii_impl.h"
#include "kii_req_impl.h"

kii_code_t _post_object(
            kii_t* kii,
            const kii_bucket_t* bucket,
            const char* object_data,
            const char* object_content_type)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "POST");

    kii_code_t ret = _set_bucket_path(kii, bucket, "/objects", "", "");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_object_content_type(kii, object_content_type);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    // Request body
    ret = _set_req_body(kii, object_data);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(code);
}

kii_code_t _put_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* opt_object_content_type,
        const char* opt_etag
        )
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_object_content_type(kii, opt_object_content_type);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_if_match(kii, opt_etag);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    ret = _set_req_body(kii, object_data);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _patch_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PATCH");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_if_match(kii, opt_etag);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    ret = _set_req_body(kii, patch_data);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _delete_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "DELETE");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_req_body(kii, "");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _get_object(
        kii_t *kii,
        const kii_bucket_t *bucket,
        const char *object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "GET");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_req_body(kii, "");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code code = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);

    return _convert_code(code);
}

kii_code_t _upload_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* body_content_type,
        size_t body_content_length)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "PUT");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "/body");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_object_body_content_type(kii, body_content_type);
    if (ret != KII_ERR_OK) {
        
        return ret;
    }

    ret = _set_content_length(kii, body_content_length);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return KII_ERR_TOO_LARGE_DATA;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code res = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(res);
}

kii_code_t _download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    khc_set_method(&kii->_khc, "GET");

    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "/body");
    if (ret != KII_ERR_OK) {
        return ret;
    }

    // Request headers.
    ret = _set_app_id_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_app_key_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_auth_header(kii);
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }
    ret = _set_req_body(kii, "");
    if (ret != KII_ERR_OK) {
        _req_headers_free_all(kii);
        return ret;
    }

    khc_set_req_headers(&kii->_khc, kii->_req_headers);
    khc_code res = khc_perform(&kii->_khc);
    _req_headers_free_all(kii);
    return _convert_code(res);
}
