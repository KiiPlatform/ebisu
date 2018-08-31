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
    // TODO: get response code.
    int respCode;
    if(respCode < 200 || 300 <= respCode) {
        goto exit;
    }

    // TODO: get buffer and its length.
    char* buff;
    size_t buff_size;
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
    // TODO: get response code.
    int respCode;
    if(respCode < 200 || 300 <= respCode) {
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
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_patch_object(
            &kii->kii_core,
            bucket,
            object_id,
            patch_data,
            opt_etag);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
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
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_replace_object(
            &kii->kii_core,
            bucket,
            object_id,
            replacement_data,
            opt_etag);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
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
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_delete_object(
            &kii->kii_core,
            bucket,
            object_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
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
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_core_get_object(
            &kii->kii_core,
            bucket,
            object_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
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
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body");
    core_err = kii_core_api_call(
            &kii->kii_core,
            "PUT",
            resource_path,
            data,
            data_length,
            body_content_type,
            NULL);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

int kii_object_init_upload_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        char* out_upload_id)
{ 
    char* buf = NULL;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];
    kii_json_field_t fields[2];
    kii_json_parse_result_t result;
    size_t buf_size = 0;

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body/uploads");
    core_err = kii_core_api_call(
            &kii->kii_core,
            "POST",
            resource_path,
            "{}",
            2,
            "application/vnd.kii.startobjectbodyuploadrequest+json",
            "accept:application/vnd.kii.startobjectbodyuploadresponse+json",
            (char*)(0));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    buf = kii->kii_core.response_body;
    buf_size = strlen(kii->kii_core.response_body);
    if (buf == NULL) {
        goto exit;
    }

    memset(fields, 0x00, sizeof(fields));
    fields[0].name = "uploadID";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = out_upload_id;
    fields[0].field_copy_buff_size = KII_UPLOADID_SIZE + 1;
    fields[1].name = NULL;

    result = prv_kii_json_read_object(kii, buf, buf_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        goto exit;
    }

    ret = 0;
exit:
    return ret;
}

int kii_object_upload_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* upload_id,
        const kii_chunk_data_t* chunk)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];
    char content_range[128];

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body/uploads/");
    strcat(resource_path, upload_id);
    strcat(resource_path, "/data");

    /* content-range */
    memset(content_range, 0x00, sizeof(content_range));
    strcpy(content_range, "Content-Range: ");
    strcat(content_range, "bytes=");
    sprintf(content_range + strlen(content_range), "%lu", (unsigned long)chunk->position);
    strcat(content_range, "-");
    sprintf(content_range + strlen(content_range), "%lu", (unsigned long)(chunk->position + chunk->length - 1));
    strcat(content_range, "/");
    sprintf(content_range + strlen(content_range), "%lu", (unsigned long)chunk->total_length);

    core_err = kii_core_api_call(
            &kii->kii_core,
            "PUT",
            resource_path,
            chunk->chunk,
            chunk->length,
            chunk->body_content_type,
            "Accept: application/json, application/*+json",
            content_range,
            (char*)(0));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

int kii_object_commit_upload(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* upload_id,
        unsigned int commit)

{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body/uploads/");
    strcat(resource_path, upload_id);
    strcat(resource_path, "/status/");
    if(commit == 0)	{
        strcat(resource_path, "cancelled");
    }
    else {
        strcat(resource_path, "committed");
    }

    core_err = kii_core_api_call(
            &kii->kii_core,
            "POST",
            resource_path,
            NULL,
            0,
            NULL,
            NULL);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

int kii_object_download_body_at_once(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        unsigned int* out_data_length)

{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body");

    core_err = kii_core_api_call(
            &kii->kii_core,
            "GET",
            resource_path,
            NULL,
            0,
            NULL,
            "Accept: */*",
            (char*)(0));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    *out_data_length = kii->kii_core._response_body_length;
    ret = 0;
exit:
    return ret;
}

int kii_object_download_body(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        unsigned int position,
        unsigned int length,
        unsigned int* out_actual_length,
        unsigned int* out_total_length)
{
    char* p1;
    char* buf;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];
    char range[128];

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    if(bucket->scope == KII_SCOPE_THING) {
        strcat(resource_path, "/things/");
        strcat(resource_path, bucket->scope_id);
    }
    strcat(resource_path, "/buckets/");
    strcat(resource_path, bucket->bucket_name);
    strcat(resource_path, "/objects/");
    strcat(resource_path, object_id);
    strcat(resource_path, "/body");

    /* Range */
    memset(range, 0x00, sizeof(range));
    strcpy(range, "Range: ");
    strcat(range, "bytes=");
    sprintf(range + strlen(range), "%d", position);
    strcat(range, "-");
    sprintf(range + strlen(range), "%d", position + length - 1);

    core_err = kii_core_api_call(
            &kii->kii_core,
            "GET",
            resource_path,
            NULL,
            0,
            NULL,
            range,
            "Accept: */*",
            (char*)(0));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    buf = kii->kii_core.http_context.buffer;
    p1 = strstr(buf, "Content-Range: ");
    if(p1 == NULL)	{
        goto exit;
    }
    p1 = strstr(p1, "/");
    if(p1 == NULL) {
        goto exit;
    }
    p1++;
    *out_total_length = atoi(p1);
    *out_actual_length = kii->kii_core._response_body_length;
    ret = 0;
exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

