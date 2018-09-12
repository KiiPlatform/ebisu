#include <string.h>
#include "kii_object_impl.h"
#include "kii_impl.h"

kii_code_t _set_bucket_path(
    kii_t* kii,
    const kii_bucket_t* bucket,
    const char* objects,
    const char* object_id,
    const char* body,
    int* path_len) {
    char *scope_strs[] = { "", "users", "groups", "things" };
    switch(bucket->scope) {
        case KII_SCOPE_APP:
            *path_len = snprintf(
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
            *path_len = snprintf(
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
    if (*path_len >= kii->_rw_buff_size) {
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

kii_code_t _make_object_content_type(
    kii_t* kii,
    const char* object_content_type,
    int* header_len)
{
    char ct_key[] = "Content-Type: ";
    char *ct_value = "application/json";
    if (object_content_type != NULL && strlen(object_content_type) > 0) {
        ct_value = (char*)object_content_type;
    }
    *header_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "%s%s", ct_key, ct_value);
    if (*header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return KII_ERR_OK;
}

kii_code_t _make_object_body_content_type(
    kii_t* kii,
    const char* object_body_content_type,
    int* header_len)
{
    char ct_key[] = "Content-Type: ";
    char *ct_value = "application/octet-stream";
    if (object_body_content_type != NULL && strlen(object_body_content_type) > 0) {
        ct_value = (char*)object_body_content_type;
    }
    *header_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "%s%s", ct_key, ct_value);
    if (*header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return KII_ERR_OK;
}

kii_code_t _kii_object_post(
            kii_t* kii,
            const kii_bucket_t* bucket,
            const char* object_data,
            const char* object_content_type)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects", "", "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "POST");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
    if (auth_len >= kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, auth_len);

    int header_len = 0;
    ret = _make_object_content_type(kii, object_content_type, &header_len);
    if (ret != KII_ERR_OK) {
        khc_slist_free_all(headers);
        return ret;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, header_len);

    // Request body
    size_t content_len = strlen(object_data);
    if (content_len + 1 > kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    strncpy(kii->_rw_buff, object_data, kii->_rw_buff_size);

    // Content-Length.
    char cl_h[128];
    int cl_h_len = snprintf(cl_h, 128, "Content-Length: %lld", (long long)content_len);
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
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "PUT");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
    if (auth_len >= kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, auth_len);

    int header_len = 0;
    ret = _make_object_content_type(kii, opt_object_content_type, &header_len);
    if (ret != KII_ERR_OK) {
        khc_slist_free_all(headers);
        return ret;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, header_len);
    if (opt_etag != NULL && strlen(opt_etag) > 0) {
        int etag_h_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "If-Match: %s", opt_etag);
        if (etag_h_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, etag_h_len);
    }

    // Request body
    size_t content_len = strlen(object_data);
    if (content_len + 1 > kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    strncpy(kii->_rw_buff, object_data, kii->_rw_buff_size);

    // Content-Length.
    char cl_h[128];
    int cl_h_len = snprintf(cl_h, 128, "Content-Length: %lld", (long long)content_len);
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

kii_code_t _patch_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "PATCH");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
    if (auth_len >= kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, auth_len);

    if (opt_etag != NULL && strlen(opt_etag) > 0) {
        int etag_h_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "If-Match: %s", opt_etag);
        if (etag_h_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, etag_h_len);
    }

    // Request body
    size_t content_len = strlen(patch_data);
    if (content_len + 1 > kii->_rw_buff_size) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    strncpy(kii->_rw_buff, patch_data, kii->_rw_buff_size);

    // Content-Length.
    char cl_h[128];
    int cl_h_len = snprintf(cl_h, 128, "Content-Length: %lld", (long long)content_len);
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

kii_code_t _delete_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "DELETE");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    if (strlen(kii->_author.access_token) > 0) {
        int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
        if (auth_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, auth_len);
    }
    headers = khc_slist_append(headers, _CONTENT_LENGTH_ZERO, strlen(_CONTENT_LENGTH_ZERO));

    khc_set_req_headers(&kii->_khc, headers);

    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code code = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);

    return _convert_code(code);
}

kii_code_t _get_object(
        kii_t *kii,
        const kii_bucket_t *bucket,
        const char *object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "GET");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    if (strlen(kii->_author.access_token) > 0) {
        int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
        if (auth_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, auth_len);
    }
    headers = khc_slist_append(headers, _CONTENT_LENGTH_ZERO, strlen(_CONTENT_LENGTH_ZERO));

    khc_set_req_headers(&kii->_khc, headers);

    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code code = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);

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
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "/body", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "PUT");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    if (strlen(kii->_author.access_token) > 0) {
        int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
        if (auth_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, auth_len);
    }

    int header_len = 0;
    ret = _make_object_body_content_type(kii, body_content_type, &header_len);
    if (ret != KII_ERR_OK) {
        khc_slist_free_all(headers);
        return ret;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, header_len);

    // Content-Length.
    char cl_h[128];
    int cl_h_len = snprintf(cl_h, 128, "Content-Length: %lld", (long long)body_content_length);
    if (cl_h_len >= 128) {
        khc_slist_free_all(headers);
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, cl_h, cl_h_len);
    khc_set_req_headers(&kii->_khc, headers);

    khc_code res = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);
    return _convert_code(res);
}

kii_code_t _download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _set_bucket_path(kii,bucket, "/objects/", object_id, "/body", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_method(&kii->_khc, "GET");

    // Request headers.
    khc_slist* headers = NULL;
    int x_app_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "X-Kii-Appid: %s", kii->_app_id);
    if (x_app_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    headers = khc_slist_append(headers, kii->_rw_buff, x_app_len);
    headers = khc_slist_append(headers, _APP_KEY_HEADER, strlen(_APP_KEY_HEADER));
    if (strlen(kii->_author.access_token) > 0) {
        int auth_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Authorization: Bearer %s", kii->_author.access_token);
        if (auth_len >= kii->_rw_buff_size) {
            khc_slist_free_all(headers);
            return KII_ERR_TOO_LARGE_DATA;
        }
        headers = khc_slist_append(headers, kii->_rw_buff, auth_len);
    }

    // Content-Length.
    headers = khc_slist_append(headers, _CONTENT_LENGTH_ZERO, strlen(_CONTENT_LENGTH_ZERO));
    khc_set_req_headers(&kii->_khc, headers);
    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code res = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);
    return _convert_code(res);
}
