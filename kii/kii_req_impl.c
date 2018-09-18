#include "kii_req_impl.h"
#include <string.h>

kii_code_t _set_bucket_path(
    kii_t* kii,
    const kii_bucket_t* bucket,
    const char* objects,
    const char* object_id,
    const char* body)
{
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

kii_code_t _set_bucket_subscription_path(kii_t* kii, const kii_bucket_t* bucket) {
    int len = 0;
    const char* scope_strs[] = { "", "users", "groups", "things" };
    switch (bucket->scope) {
        case KII_SCOPE_APP: {
            const char path[] = "/api/apps/%s/buckets/%s/filters/all/push/subscriptions/things/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path, kii->_app_id, bucket->bucket_name, kii->_author.author_id);
            break;
        }
        case KII_SCOPE_USER:
        case KII_SCOPE_GROUP:
        case KII_SCOPE_THING: {
            const char* scope = scope_strs[bucket->scope];
            const char path[] = "/api/apps/%s/%s/%s/buckets/%s/filters/all/push/subscriptions/things/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path,
                kii->_app_id, scope, bucket->scope_id, bucket->bucket_name, kii->_author.author_id);
            break;
        }
        default:
            return KII_ERR_FAIL;
    }
    if (len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_code kcode = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (kcode != KHC_ERR_OK) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return KII_ERR_OK;
}

kii_code_t _set_topic_path(
    kii_t* kii,
    const kii_topic_t* topic)
{
    int len = 0;
    const char* scope_strs[] = { "", "users", "groups", "things" };
    switch (topic->scope) {
        case KII_SCOPE_APP: {
            const char path[] = "/api/apps/%s/topics/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path, kii->_app_id, topic->topic_name);
            break;
        }
        case KII_SCOPE_USER:
        case KII_SCOPE_GROUP:
        case KII_SCOPE_THING: {
            const char* scope = scope_strs[topic->scope];
            const char path[] = "/api/apps/%s/%s/%s/topics/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path,
                kii->_app_id, scope, topic->scope_id, topic->topic_name);
            break;
        }
        default:
            return KII_ERR_FAIL;
    }
    if (len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_code kcode = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (kcode != KHC_ERR_OK) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return KII_ERR_OK;
}

kii_code_t _set_topic_subscription_path(kii_t* kii, const kii_topic_t* topic) {
    int len = 0;
    const char* scope_strs[] = { "", "users", "groups", "things" };
    switch (topic->scope) {
        case KII_SCOPE_APP: {
            const char path[] = "/api/apps/%s/topics/%s/push/subscriptions/things/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path, kii->_app_id, topic->topic_name, kii->_author.author_id);
            break;
        }
        case KII_SCOPE_USER:
        case KII_SCOPE_GROUP:
        case KII_SCOPE_THING: {
            const char* scope = scope_strs[topic->scope];
            const char path[] = "/api/apps/%s/%s/%s/topics/%s/push/subscriptions/things/%s";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path,
                kii->_app_id, scope, topic->scope_id, topic->topic_name, kii->_author.author_id);
            break;
        }
        default:
            return KII_ERR_FAIL;
    }
    if (len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_code kcode = khc_set_path(&kii->_khc, kii->_rw_buff);
    if (kcode != KHC_ERR_OK) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return KII_ERR_OK;
}

kii_code_t _set_content_type(
    kii_t* kii,
    const char* content_type)
{
    int header_len = snprintf(kii->_rw_buff, kii->_rw_buff_size, "Content-Type: %s", content_type);
    if (header_len >= kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
    return KII_ERR_OK;
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
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
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
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
    return KII_ERR_OK;
}

kii_code_t _set_content_length(
    kii_t* kii,
    size_t content_length)
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
    khc_slist* list = khc_slist_append(kii->_req_headers, cl_h, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_rw_buff_req_size = content_length;
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
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
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
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
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
        khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
        if (list == NULL) {
            return KII_ERR_ALLOCATION;
        }
        kii->_req_headers = list;
        return KII_ERR_OK;
    } else {
        // Nothing to do.
        return KII_ERR_OK;
    }
}

kii_code_t _set_if_match(kii_t* kii, const char* etag)
{
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
    khc_slist* list = khc_slist_append(kii->_req_headers, kii->_rw_buff, header_len);
    if (list == NULL) {
        return KII_ERR_ALLOCATION;
    }
    kii->_req_headers = list;
    return KII_ERR_OK;
}

kii_code_t _set_req_body(kii_t* kii, const char* body_contents)
{
    size_t content_len = strlen(body_contents);
    if (content_len + 1 > kii->_rw_buff_size) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    strncpy(kii->_rw_buff, body_contents, kii->_rw_buff_size);
    
    return _set_content_length(kii, content_len);
}