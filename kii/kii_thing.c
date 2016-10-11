#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json_utils.h"
#include "kii_core_hidden.h"

#include <kii_core.h>

#define KII_SDK_INFO "sn=te;sv=1.1.6"

int _kii_init_with_info(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key,
        const char* info)
{
    memset(kii, 0x00, sizeof(kii_t));
    return _kii_core_init_with_info(&(kii->kii_core), site, app_id,
            app_key, info) == KIIE_FAIL ? -1 : 0;
}
/*
int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key)
{
    return _kii_init_with_info(kii, site, app_id, app_key, KII_SDK_INFO);
}
*/
int kii_thing_authenticate(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    char* buf;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[3];
    kii_json_parse_result_t result;
    size_t buf_size = 0;

    core_err = kii_core_thing_authentication(&kii->kii_core, vendor_thing_id, password);
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
        ret = -1;
        goto exit;
    }

    memset(fields, 0, sizeof(fields));
    fields[0].name = "id";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->kii_core.author.author_id;
    fields[0].field_copy_buff_size = sizeof(kii->kii_core.author.author_id) /
            sizeof(kii->kii_core.author.author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->kii_core.author.access_token;
    fields[1].field_copy_buff_size = sizeof(kii->kii_core.author.access_token) /
            sizeof(kii->kii_core.author.access_token[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buf, buf_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

int kii_thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    char* buf = NULL;
    size_t buf_size = 0;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[3];
    kii_json_parse_result_t result;

    core_err = kii_core_register_thing_with_id(&kii->kii_core, vendor_thing_id,
            password, thing_type);
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
    /* parse response */
    buf = kii->kii_core.response_body;
    buf_size = strlen(kii->kii_core.response_body);
    if (buf == NULL) {
        ret = -1;
        goto exit;
    }

    memset(fields, 0, sizeof(fields));
    fields[0].name = "_accessToken";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->kii_core.author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->kii_core.author.access_token) /
            sizeof(kii->kii_core.author.access_token[0]);
    fields[1].name = "_thingID";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->kii_core.author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->kii_core.author.author_id) /
            sizeof(kii->kii_core.author.author_id[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buf, buf_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
