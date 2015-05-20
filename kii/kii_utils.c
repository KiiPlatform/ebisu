#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jsmn.h>

#include "kii_utils.h"

static int prv_jsmn_token_num(const char* json_string, size_t json_string_len)
{
    jsmn_parser parser;

    assert(json_string != NULL);
    assert(json_string_len >= 0);

    jsmn_init(&parser);

    return jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
}

int prv_kii_jsmn_get_tokens(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        jsmntok_t* tokens,
        size_t token_num)
{
    jsmn_parser parser;
    int ret = -1;
    int parse_result = JSMN_ERROR_NOMEM;

    assert(json_string != NULL);
    assert(tokens != NULL);


    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, json_string, json_string_len, tokens,
            token_num);
    if (parse_result >= 0) {
        ret = 0;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        ret = -1;
        M_KII_LOG(kii->kii_core.logger_cb(
            "Not enough tokens were provided\r\n"));
    } else if (parse_result == JSMN_ERROR_INVAL) {
        ret = -1;
        M_KII_LOG(kii->kii_core.logger_cb(
            "Invalid character inside JSON string\r\n"));
    } else if (parse_result == JSMN_ERROR_PART) {
        ret = -1;
        M_KII_LOG(kii->kii_core.logger_cb(
            "The string is not a full JSON packet, more bytes expected\r\n"));
    } else {
        ret = -1;
        M_KII_LOG(kii->kii_core.logger_cb(
            "Unexpected error: %d\r\n", parse_result));
    }

    return ret;
}

int prv_kii_jsmn_get_value(
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token)
{
    int i = 0;
    int index = 1;
    int ret = -1;

    assert(json_string != NULL);
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
        if (strcmp(name, json_string + key_token->start) == 0) {
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
                {
                    int num = prv_jsmn_token_num(
                            json_string + value_token->start,
                            json_string_len - value_token->start);
                    if (num < 0) {
                        ret = -1;
                        goto exit;
                    }
                    index += (num + 1);
                }
                break;
        }
    }

exit:
    return ret;
}

