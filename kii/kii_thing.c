#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <jsmn.h>

#include "kii.h"
#include "kii_json.h"
#include "kii_util.h"

#include "kii_core.h"

/* This define remove after. */
#define KII_JSON_TOKEN_NUM 128

int kii_thing_authenticate(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    char* buf;
    char* start_body;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    kii_json_field_t fields[] = {
        { "id" },
        { "access_token" },
        { NULL }
    };
    kii_json_parse_result_t result;
    size_t buf_size = 0;
    size_t field_len = 0;

    buf = kii->kii_core.http_context.buffer;
    buf_size = kii->kii_core.http_context.buffer_size;
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
    if (fields[1].result != KII_JSON_FIELD_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }

    field_len = fields[0].end - fields[0].start;
    memcpy(kii->kii_core.author.author_id, start_body + fields[0].start,
            field_len);
    kii->kii_core.author.author_id[field_len] = '\0';

    field_len = fields[1].end - fields[1].start;
    memcpy(kii->kii_core.author.access_token, start_body + fields[1].start,
            field_len);
    kii->kii_core.author.access_token[field_len] = '\0';

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
    char thing_data[1024];
    jsmntok_t tokens[KII_JSON_TOKEN_NUM];
    jsmntok_t* access_token = NULL;
    size_t buf_size = 0;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

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
    ret = prv_kii_jsmn_get_tokens(kii, buf, buf_size, tokens,
            sizeof(tokens) / sizeof(tokens[0]));
    if (ret != 0) {
        goto exit;
    }
    if (tokens[0].type != JSMN_OBJECT || tokens[0].size < 2) {
        goto exit;
    }
    ret = prv_kii_jsmn_get_value(buf, buf_size, tokens, "access_token",
            &access_token);
    if (ret != 0) {
        goto exit;
    }
    if (access_token == NULL) {
        goto exit;
    }
    memcpy(kii->kii_core.author.access_token, buf + access_token->start,
            access_token->end - access_token->start);
    strcpy(kii->kii_core.author.author_id, vendor_thing_id);
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
