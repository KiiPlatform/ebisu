#ifndef __kii_object_impl__
#define __kii_object_impl__

#ifdef __cplusplus
extern "C" {
#endif

#include "kii.h"

kii_code_t _make_bucket_path(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *objects,
    const char *object_id,
    const char *body,
    int *path_len);

kii_code_t _make_object_content_type(
    kii_t *kii,
    const char *object_content_type,
    int *header_len);

kii_code_t _make_object_body_content_type(
    kii_t *kii,
    const char *object_body_content_type,
    int *header_len);

kii_code_t _post_object(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *object_data,
    const char *object_content_type);

kii_code_t _put_object(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *object_id,
    const char *object_data,
    const char *opt_object_content_type,
    const char *opt_etag);

kii_code_t _patch_object(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *object_id,
    const char *patch_data,
    const char *opt_etag);

kii_code_t _get_object(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *object_id);

kii_code_t _delete_object(
    kii_t *kii,
    const kii_bucket_t *bucket,
    const char *object_id);

kii_code_t _upload_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* body_content_type,
        size_t body_content_length);

kii_code_t _download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id);

#ifdef __cplusplus
}
#endif

#endif