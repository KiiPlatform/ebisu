#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jsmn.h>

#include "kii.h"

#include "kii_core.h"

static int prv_jsmn_get_tokens(
        const char* javascript,
        size_t javascript_len,
        jsmntok_t** out_tokens)
{
    jsmn_parser parser;
    int ret = -1;
    int parse_result = JSMN_ERROR_NOMEM;
    jsmntok_t* tokens = NULL;
    int len = 0;

    assert(javascript != NULL);
    assert(out_tokens != NULL);

    jsmn_init(&parser);

    len = jsmn_parse(&parser, javascript, javascript_len, NULL, 0);
    if (len <= 0) {
        ret = -1;
        goto exit;
    }

    tokens = malloc(sizeof(jsmntok_t) * len);
    if (tokens == NULL) {
        ret = -1;
        goto exit;
    }

    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, javascript, javascript_len, tokens, len);
    if (parse_result <= 0) {
        ret = -1;
        goto exit;
    }
    ret = 0;

exit:
    if (ret != 0) {
        free(tokens);
        tokens = NULL;
    }
    *out_tokens = tokens;
    return ret;
}

static int prv_jsmn_get_child_size(const jsmntok_t* root)
{
    int i = 0;
    int ret = 0;

    assert(root != NULL);
    assert(root->type == JSMN_OBJECT || root->type == JSMN_ARRAY);

    if (root->size > 0) {
        const jsmntok_t* children = &root[1];
        for (i = 0; i < root->size; ++i) {
            switch (children[i].type) {
                case JSMN_STRING:
                case JSMN_PRIMITIVE:
                    ++ret;
                    break;
                case JSMN_OBJECT:
                case JSMN_ARRAY:
                    ret += (prv_jsmn_get_child_size(&children[i]) + 1);
                    break;
            }
        }
    }
    return ret;
}

static int prv_jsmn_get_value(
        const char* javascript,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token)
{
    int i = 0;
    int index = 1;
    int ret = -1;

    assert(javascript != NULL);
    assert(tokens != NULL);
    assert(name != NULL && strlen(name) > 0);
    assert(out_token != NULL);

    if (tokens[0].type != JSMN_OBJECT && tokens[0].size < 2) {
        goto exit;
    }

    for (i = 0; i < tokens[0].size; ++i) {
        const jsmntok_t* key_token = tokens + index;
        const jsmntok_t* value_token = tokens + index + 1;
        if (key_token->type != JSMN_STRING) {
            goto exit;
        }
        if (strcmp(name, javascript + key_token->start) == 0) {
            ret = 0;
            *out_token = (jsmntok_t*)value_token;
            break;
        }
        switch (value_token->type) {
            case JSMN_STRING:
            case JSMN_PRIMITIVE:
                index += 2;
                break;
            case JSMN_OBJECT:
            case JSMN_ARRAY:
                index += (prv_jsmn_get_child_size(value_token) + 2);
                break;
        }
    }

exit:
    return ret;
}

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

    ret = prv_jsmn_get_tokens(buf, buf_size, &tokens);
    if (ret != 0) {
        goto exit;
    }
    if (tokens[0].type != JSMN_OBJECT || tokens[0].size < 2) {
        goto exit;
    }
    ret = prv_jsmn_get_value(buf, tokens, "access_token", &access_token);
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
