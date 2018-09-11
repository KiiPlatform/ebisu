#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json_utils.h"
#include "kii_impl.h"

static kii_code_t _make_bucket_path(
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
        return KII_ERR_OK;
    }
}

static kii_code_t _make_object_content_type(
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

static kii_code_t _make_object_body_content_type(
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

static kii_code_t _kii_object_post(
            kii_t* kii,
            const kii_bucket_t* bucket,
            const char* object_data,
            const char* object_content_type)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects", "", "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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

static kii_code_t _kii_object_put(
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
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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

static kii_code_t _patch_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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

static kii_code_t _delete_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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
    const char cl0[] = "Content-Length: 0";
    headers = khc_slist_append(headers, cl0, strlen(cl0));

    khc_set_req_headers(&kii->_khc, headers);

    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code code = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);

    return _convert_code(code);
}

static kii_code_t _get_object(
        kii_t *kii,
        const kii_bucket_t *bucket,
        const char *object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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
    const char cl0[] = "Content-Length: 0";
    headers = khc_slist_append(headers, cl0, strlen(cl0));

    khc_set_req_headers(&kii->_khc, headers);

    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code code = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);

    return _convert_code(code);
}

kii_code_t kii_object_post(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_data,
        const char* object_content_type,
        char* out_object_id)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);
    kii_code_t ret = _kii_object_post(
            kii,
            bucket,
            object_data,
            object_content_type);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        ret = KII_ERR_FAIL;
        goto exit;
    }

    kii_json_field_t fields[2];
    kii_json_parse_result_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "objectID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_object_id;
    fields[0].field_copy_buff_size = KII_OBJECTID_SIZE + 1;
    fields[1].name = NULL;

    result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = KII_ERR_PARSE_JSON;
        goto exit;
    }

    ret = KII_ERR_OK;

exit:
    return ret;
}

kii_code_t kii_object_put(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* object_content_type,
        const char* opt_etag)
{
    _reset_buff(kii);
    khc_set_zero_excl_cb(&kii->_khc);
    kii_code_t ret = _kii_object_put(
            kii,
            bucket,
            object_id,
            object_data,
            object_content_type,
            opt_etag);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }
    ret = KII_ERR_OK;

exit:
    return ret;

}

kii_code_t kii_object_patch(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    _reset_buff(kii);
    khc_set_zero_excl_cb(&kii->_khc);
    kii_code_t code = _patch_object(
            kii,
            bucket,
            object_id,
            patch_data,
            opt_etag);
    if (code != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        code = KII_ERR_RESP_STATUS;
        goto exit;
    }

    code = KII_ERR_OK;

exit:
    return code;	
}

kii_code_t kii_object_delete(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    _reset_buff(kii);
    khc_set_zero_excl_cb(&kii->_khc);
    kii_code_t res = _delete_object(
            kii,
            bucket,
            object_id);
    if (res != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        res = KII_ERR_RESP_STATUS;
        goto exit;
    }

    res = KII_ERR_OK;
exit:
    return res;
}

kii_code_t kii_object_get(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);
    kii_code_t ret = _get_object(
            kii,
            bucket,
            object_id);
    if (ret != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    ret = KII_ERR_OK;
exit:
    return ret;
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
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "/body", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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

kii_code_t kii_object_upload_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* body_content_type,
        size_t body_content_length,
        const KII_CB_READ read_cb,
        void* userdata)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);
    khc_set_cb_read(&kii->_khc, read_cb, userdata);

    kii_code_t ret = _upload_body(kii, bucket, object_id, body_content_type, body_content_length);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    // Restore default callback.
    khc_set_cb_read(&kii->_khc, _cb_read_buff, kii);
    return ret;
}

kii_code_t _download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    khc_set_host(&kii->_khc, kii->_app_host);
    int path_len = 0;
    kii_code_t ret = _make_bucket_path(kii,bucket, "/objects/", object_id, "/body", &path_len);
    if (ret != KII_ERR_OK) {
        return ret;
    }
    khc_set_path(&kii->_khc, kii->_rw_buff);
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
    char cl0[] = "Content-Length: 0";
    headers = khc_slist_append(headers, cl0, strlen(cl0));
    khc_set_req_headers(&kii->_khc, headers);
    kii->_rw_buff[0] = '\0';
    _kii_set_content_length(kii, 0);

    khc_code res = khc_perform(&kii->_khc);
    khc_slist_free_all(headers);
    return _convert_code(res);
}

kii_code_t kii_object_download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const KII_CB_WRITE write_cb,
        void* userdata)

{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);
    khc_set_cb_write(&kii->_khc, write_cb, userdata);

    kii_code_t ret = _download_body(kii, bucket, object_id);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

exit:
    // Restore default callback.
    khc_set_cb_write(&kii->_khc, _cb_write_buff, kii);
    return ret;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

