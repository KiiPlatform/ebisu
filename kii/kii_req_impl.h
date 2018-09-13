#ifndef __kii_req_impl__
#define __kii_req_impl__

#include "kii.h"

kii_code_t _set_bucket_path(
    kii_t* kii,
    const kii_bucket_t* bucket,
    const char* objects,
    const char* object_id,
    const char* body);

kii_code_t _set_bucket_subscription_path(
    kii_t* kii,
    const kii_bucket_t* bucket);

kii_code_t _set_topic_path(
    kii_t* kii,
    const kii_topic_t* topic);

kii_code_t _set_topic_subscription_path(
    kii_t* kii,
    const kii_topic_t* topic);

kii_code_t _set_object_content_type(
    kii_t* kii,
    const char* object_content_type);

kii_code_t _set_object_body_content_type(
    kii_t* kii,
    const char* object_body_content_type);

kii_code_t _set_content_type(
    kii_t* kii,
    const char* content_type);

kii_code_t _set_content_length(
    kii_t* kii,
    size_t content_length);

kii_code_t _set_app_id_header(kii_t* kii);

kii_code_t _set_app_key_header(kii_t* kii);

kii_code_t _set_auth_header(kii_t* kii);

kii_code_t _set_if_match(kii_t* kii, const char* etag);

kii_code_t _set_req_body(kii_t* kii, const char* body_contents);

#endif
