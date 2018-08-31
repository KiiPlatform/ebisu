#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json_utils.h"

static khc_code _create_new_object(
            kii_t* kii,
            const kii_bucket_t* bucket,
            const char* object_data,
            const char* object_content_type)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _create_new_object_with_id(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* opt_object_content_type
        )
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _patch_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _replace_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* replace_data,
        const char* opt_etag)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _delete_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _get_object(
        kii_t *kii,
        const kii_bucket_t *bucket,
        const char *object_id)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

int kii_object_create(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_data,
        const char* object_content_type,
        char* out_object_id)
{
    int ret = -1;
    khc_code khc_err = _create_new_object(
            kii,
            bucket,
            object_data,
            object_content_type);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    // TODO: get buffer and its length.
    char* buff = NULL;
    size_t buff_size = 0;
    if (buff == NULL) {
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
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

int kii_object_create_with_id(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* object_content_type)
{
    int ret = -1;

    khc_code khc_err = _create_new_object_with_id(
            kii,
            bucket,
            object_id,
            object_data,
            object_content_type);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }
    ret = 0;

exit:
    return ret;

}

int kii_object_patch(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
    int ret = -1;
    khc_code khc_err = _patch_object(
            kii,
            bucket,
            object_id,
            patch_data,
            opt_etag);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    ret = 0;

exit:
    return ret;	
}

int kii_object_replace(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* replacement_data,
        const char* opt_etag)
{
    int ret = -1;

    khc_code khc_err = _replace_object(
            kii,
            bucket,
            object_id,
            replacement_data,
            opt_etag);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    ret = 0;

exit:
    return ret;	
}

int kii_object_delete(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    int ret = -1;

    khc_code khc_err = _delete_object(
            kii,
            bucket,
            object_id);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

int kii_object_get(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
    int ret = -1;

    khc_code khc_err = _get_object(
            kii,
            bucket,
            object_id);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

int kii_object_upload_body_at_once(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* body_content_type,
        const void* data,
        size_t data_length)
{
    // TODO: This API should be refined.
    return -1;
}

int kii_object_download_body_at_once(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        unsigned int* out_data_length)

{
    // TODO: This API should be refined.
    return -1;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

