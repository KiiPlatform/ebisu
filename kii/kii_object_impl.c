#include <string.h>
#include "kii_object_impl.h"
#include "kii_impl.h"

kii_code_t _set_bucket_path(
    kii_t* kii,
    const kii_bucket_t* bucket,
    const char* objects,
    const char* object_id,
    const char* body
    ) {
    char *scope_strs[] = { "", "users", "groups", "things" };
    int path_len = 0;
    switch(bucket->scope) {
        case KII_SCOPE_APP:
            path_len = snprintf(
                kii->_rw_buff,
                kii->_rw_buff_size,
                "/api/apps/%s/buckets/%s%s%s%s",
                kii->_app_id,
                bucket->bucket_name,
                objects,
                object_id,
                body);
            break;
        case KII_SCOPE_USER:
        case KII_SCOPE_GROUP:
        case KII_SCOPE_THING:
            path_len = snprintf(
                kii->_rw_buff,
                kii->_rw_buff_size,
                "/api/apps/%s/%s/%s/buckets/%s%s%s%s",
                kii->_app_id,
                scope_strs[bucket->scope],
                bucket->scope_id,
                bucket->bucket_name,
                objects,
                object_id,
                body);
            break;
        default:
            return KII_ERR_FAIL;
    }
    if (path_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    } else {
        khc_code ret = khc_set_path(&kii->_khc, kii->_rw_buff);
        if (ret == KHC_ERR_OK) {
            return KII_ERR_OK;
        } else {
            return KII_ERR_TOO_LARGE_DATA;
        }
    }
}

kii_code_t _set_object_content_type(
    kii_t* kii,
    const char* object_content_type)
{
    char ct_key[] = "Content-Type: ";
    char *ct_value = "application/json";
    if (object_content_type != NULL && strlen(object_content_type) > 0) {
        ct_value = (char*)object_content_type;
    }
    int header_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "%s%s", ct_key, ct_value);
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    return KII_ERR_OK;
}

kii_code_t _set_object_body_content_type(
    kii_t* kii,
    const char* object_body_content_type)
{
    char ct_key[] = "Content-Type: ";
    char *ct_value = "application/octet-stream";
    if (object_body_content_type != NULL && strlen(object_body_content_type) > 0) {
        ct_value = (char*)object_body_content_type;
    }
    int header_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "%s%s", ct_key, ct_value);
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    return KII_ERR_OK;
}

kii_code_t _set_content_length(
    kii_t* kii,
    size_t content_length
)
{
    size_t cl_size = 128;
    char cl_h[cl_size];
    int header_len = snprintf(
        cl_h,
        cl_size,
        "Content-Length: %lld",
        (long long)content_length);
    if (header_len >= cl_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, cl_h, header_len);
    _kii_set_content_length(kii, content_length);
    return KII_ERR_OK;
}

kii_code_t _set_app_id_header(kii_t* kii)
{
    int header_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "X-Kii-Appid: %s",
        kii->_app_id);
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    return KII_ERR_OK;
}

kii_code_t _set_app_key_header(kii_t* kii)
{
    int header_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "X-Kii-Appkey: %s",
        "k");
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    return KII_ERR_OK;
}

kii_code_t _set_auth_header(kii_t* kii)
{
    if (strlen(kii->_author.access_token) > 0) {
        int header_len = snprintf(
            kii->_rw_buff,
            kii->_rw_buff_size,
            "Authorization: Bearer %s",
            kii->_author.access_token);
        if (header_len >= kii->_rw_buff_size) {
            return KII_ERR_TOO_LARGE_DATA;
        }
        kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
        return KII_ERR_OK;
    } else {
        // Nothing to do.
        return KII_ERR_OK;
    }
}

kii_code_t _set_if_match(kii_t* kii, const char* etag) {
    if (etag == NULL || strlen(etag) == 0) {
        // Skip.
        return KII_ERR_OK;
    }
    int header_len = snprintf(
        kii->_rw_buff,
        kii->_rw_buff_size,
        "If-Match: %s",
        etag);
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    kii->_req_headers = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    return KII_ERR_OK;
}

kii_code_t _set_req_body(kii_t* kii, const char* body_contents) {
    size_t content_len = strlen(body_contents);
    if (content_len + 1 > kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    strncpy(kii->_rw_buff, body_contents, kii->_rw_buff_size);
    
    return _set_content_length(kii, content_len);
}

kii_code_t _kii_object_post(
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

kii_code_t _kii_object_put(
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
