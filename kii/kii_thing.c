#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <jsmn.h>

#include "kii.h"
#include "kii_utils.h"

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
    jsmntok_t* tokens = NULL;
    jsmntok_t* access_token = NULL;
    size_t buf_size = 0;

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

    ret = prv_kii_jsmn_get_tokens(buf, buf_size, &tokens);
    if (ret != 0) {
        goto exit;
    }
    if (tokens[0].type != JSMN_OBJECT || tokens[0].size < 2) {
        goto exit;
    }
    ret = prv_kii_jsmn_get_value(buf, buf_size, tokens, "access_token",
            &access_token);
    if (access_token == NULL) {
        goto exit;
    }

    strcpy(kii->kii_core.author.author_id, vendor_thing_id);
    memcpy(kii->kii_core.author.access_token, buf + access_token->start,
            access_token->end - access_token->start);
    ret = 0;

exit:
    if (tokens != NULL) {
        free(tokens);
    }
    return ret;
}

int kii_thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    char* buf;
    char* p1;
    char* p2;
    char thing_data[1024];
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->kii_core.http_context.buffer;
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
    p1 = strstr(buf, "\"_accessToken\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL) {
        goto exit;
    }
    memcpy(kii->kii_core.author.access_token, p1, p2 - p1);
    strcpy(kii->kii_core.author.author_id, vendor_thing_id);
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
