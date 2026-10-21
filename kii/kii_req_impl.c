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

kii_code_t _set_topic_push_message_path(kii_t* kii, const kii_topic_t* topic) {
    int len = 0;
    const char* scope_strs[] = { "", "users", "groups", "things" };
    switch (topic->scope) {
        case KII_SCOPE_APP: {
            const char path[] = "/api/apps/%s/topics/%s/push/messages";
            len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path, kii->_app_id, topic->topic_name);
            break;
        }
        case KII_SCOPE_USER:
        case KII_SCOPE_GROUP:
        case KII_SCOPE_THING: {
            const char* scope = scope_strs[topic->scope];
            const char path[] = "/api/apps/%s/%s/%s/topics/%s/push/messages";
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

kii_code_t _set_thing_push_message_path(kii_t* kii, const char* thing_id) {
    const char path[] = "/api/apps/%s/things/%s/push/messages";
    int len = snprintf(kii->_rw_buff, kii->_rw_buff_size, path, kii->_app_id, thing_id);
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

// Writes the base64 encoding of src's src_len bytes to dst, terminated. Returns
// the encoding's length, or -1 if it doesn't fit in dst_size.
static int _base64_encode(const char* src, size_t src_len, char* dst, size_t dst_size)
{
    static const char digits[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = (src_len + 2) / 3 * 4;
    if (out_len >= dst_size) {
        return -1;
    }
    const unsigned char* in = (const unsigned char*)src;
    char* out = dst;
    size_t i = 0;
    for (; i + 2 < src_len; i += 3) {
        uint32_t n = ((uint32_t)in[i] << 16) | ((uint32_t)in[i + 1] << 8) | in[i + 2];
        *out++ = digits[(n >> 18) & 0x3f];
        *out++ = digits[(n >> 12) & 0x3f];
        *out++ = digits[(n >> 6) & 0x3f];
        *out++ = digits[n & 0x3f];
    }
    if (i < src_len) {
        uint32_t n = (uint32_t)in[i] << 16;
        if (i + 1 < src_len) {
            n |= (uint32_t)in[i + 1] << 8;
        }
        *out++ = digits[(n >> 18) & 0x3f];
        *out++ = digits[(n >> 12) & 0x3f];
        *out++ = (i + 1 < src_len) ? digits[(n >> 6) & 0x3f] : '=';
        *out++ = '=';
    }
    *out = '\0';
    return (int)out_len;
}

kii_code_t _set_auth_basic_app(kii_t* kii)
{
    // The app's ID authorizes the request. Its key isn't needed, so a
    // placeholder stands in for it.
    char credentials[KII_APP_ID_MAX_SIZE + sizeof(":dummy")];
    int cred_len = snprintf(credentials, sizeof(credentials), "%s:dummy", kii->_app_id);
    if (cred_len < 0 || (size_t)cred_len >= sizeof(credentials)) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    static const char prefix[] = "Basic ";
    if (kii->_rw_buff_size < sizeof(prefix)) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    memcpy(kii->_rw_buff, prefix, sizeof(prefix));
    if (_base64_encode(credentials, (size_t)cred_len, kii->_rw_buff + sizeof(prefix) - 1,
                kii->_rw_buff_size - (sizeof(prefix) - 1)) < 0) {
        return KII_ERR_TOO_LARGE_DATA;
    }
    return kii_api_call_append_header(kii, "Authorization", kii->_rw_buff);
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

