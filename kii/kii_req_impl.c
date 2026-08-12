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
    return kii_api_call_append_header(kii, "Content-Type", content_type);
}

kii_code_t _set_content_encoding(
        kii_t* kii,
        const char* content_encoding)
{
    return kii_api_call_append_header(kii, "Content-Encoding", content_encoding);
}

kii_code_t _set_object_content_type(
        kii_t* kii,
        const char* object_content_type)
{
    return kii_api_call_append_header_or_default(kii, "Content-Type", object_content_type, "application/json");
}

kii_code_t _set_object_body_content_type(
        kii_t* kii,
        const char* object_body_content_type)
{
    return kii_api_call_append_header_or_default(kii, "Content-Type", object_body_content_type, "application/octet-stream");
}

kii_code_t _set_content_length(
        kii_t* kii,
        size_t content_length)
{
    char length_value[KII_REQ_CONTENT_LENGTH_MAX_SIZE];
    snprintf(length_value, KII_REQ_CONTENT_LENGTH_MAX_SIZE, "%lld", (long long)content_length);
    return kii_api_call_append_header(kii, "Content-Length", length_value);
}

kii_code_t _set_app_id_header(kii_t* kii)
{
    return kii_api_call_append_header(kii, "X-Kii-Appid", kii->_app_id);
}

kii_code_t _set_app_key_header(kii_t* kii)
{
    return kii_api_call_append_header(kii, "X-Kii-Appkey", "k");
}

kii_code_t _set_auth_header(kii_t* kii)
{
    return _set_auth_bearer_token(kii, kii->_author.access_token);
}

kii_code_t _set_auth_bearer_token(kii_t* kii, const char* token)
{
    if (strlen(token) > 0) {
        kii->_rw_buff[0] = '\0';
        int header_len = snprintf(
                kii->_rw_buff,
                kii->_rw_buff_size,
                "Bearer %s",
                token);
        if (header_len >= kii->_rw_buff_size) {
            return KII_ERR_TOO_LARGE_DATA;
        }
        return kii_api_call_append_header(kii, "Authorization", kii->_rw_buff);
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
    return kii_api_call_append_header(kii, "If-Match", etag);
}

kii_code_t _set_req_body(kii_t* kii, const char* body_contents)
{
    if(body_contents == NULL) {
        return kii_api_call_set_no_body(kii);
    }

    size_t content_len = strlen(body_contents);
    if(content_len == 0) {
        return kii_api_call_set_no_body(kii);
    }
    else {
        return kii_api_call_append_body(kii, body_contents, content_len);
    }
}

kii_code_t _set_m_0_header(kii_t* kii)
{
    return kii_api_call_append_header(kii, "kii-m", "0");
}

