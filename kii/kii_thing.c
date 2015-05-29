#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json.h"
#include "kii_util.h"

#include "kii_core.h"

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
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }

    buf = kii->kii_core.response_body;
    buf_size = kii->kii_core.http_context.buffer_size -
            (kii->kii_core.http_context.buffer - kii->kii_core.response_body);
    if (buf == NULL) {
        ret = -1;
        goto exit;
    }

    memset(fields, sizeof(fields[0]), sizeof(fields) / sizeof(fields[0]));
    fields[0].name = "id";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy_buff = kii->kii_core.author.author_id;
    fields[0].field_copy_buff_size = sizeof(kii->kii_core.author.author_id) /
            sizeof(kii->kii_core.author.author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy_buff = kii->kii_core.author.access_token;
    fields[1].field_copy_buff_size = sizeof(kii->kii_core.author.access_token) /
            sizeof(kii->kii_core.author.access_token[0]);
    fields[2].name = NULL;

    result = kii_json_read_object(kii, buf, buf_size, fields);
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
    char* buf;
    char* start_body;
    char thing_data[1024];
    size_t buf_size = 0;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[] = {
        { "access_token" },
        { NULL }
    };
    kii_json_parse_result_t result;
    size_t field_len = 0;

    buf = kii->kii_core.http_context.buffer;
    buf_size = kii->kii_core.http_context.buffer_size;
    sprintf(thing_data,
            "{\"_vendorThingID\":\"%s\",\"_thingType\":\"%s\",\"_password\":\"%s\"}",
            vendor_thing_id, thing_type, password);
    core_err = kii_core_register_thing(&kii->kii_core, thing_data);
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
    start_body = prv_kii_util_get_http_body(buf, buf_size);
    if (start_body == NULL) {
        ret = -1;
        goto exit;
    }
    result = kii_json_read_object(kii, start_body,
            buf_size - (start_body - buf), fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }
    if (fields[0].result != KII_JSON_FIELD_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }
    field_len = fields[0].end - fields[0].start;
    memcpy(kii->kii_core.author.access_token, start_body + fields[0].start,
            field_len);
    kii->kii_core.author.access_token[field_len] = '\0';
    strcpy(kii->kii_core.author.author_id, vendor_thing_id);
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
