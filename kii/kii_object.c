#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json_utils.h"
#include "kii_impl.h"
#include "kii_object_impl.h"

kii_code_t kii_post_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_data,
        const char* object_content_type,
        kii_object_id_t* out_object_id)
{
    khc_set_zero_excl_cb(&kii->_khc);
    _reset_buff(kii);
    kii_code_t ret = _post_object(
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
    fields[0].field_copy.string = out_object_id->id;
    fields[0].field_copy_buff_size = sizeof(out_object_id->id);
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

kii_code_t kii_put_object(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* object_content_type,
        const char* opt_etag)
{
    _reset_buff(kii);
    khc_set_zero_excl_cb(&kii->_khc);
    kii_code_t ret = _put_object(
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

kii_code_t kii_patch_object(
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

kii_code_t kii_delete_object(
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
    if (res != KII_ERR_OK) {
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

kii_code_t kii_get_object(
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

kii_code_t kii_upload_object_body(
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

kii_code_t kii_download_object_body(
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
