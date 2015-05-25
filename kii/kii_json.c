#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jsmn.h>

#include "kii_json.h"

#define TOKEN_NUM 64

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

static kii_json_parse_result_t prv_check_object_fields(
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        kii_json_field_t* fields)
{
    int i;

    switch (tokens[0].type)
    {
        case JSMN_ARRAY:
            return KII_JSON_PARSE_UNEXPECTED_ARRAY;
        case JSMN_OBJECT:
            break;
        default:
            return KII_JSON_PARSE_INVALID;
    }

    for (i = 0; fields[i].name != NULL; ++i)
    {
        jsmntok_t* value = NULL;
        int result = 0;

        result = prv_kii_jsmn_get_value(json_string, json_string_len, tokens,
                fields[i].name, &value);
        if (result != 0 || value == NULL)
        {
            fields[i].result = KII_JSON_FIELD_PARSE_NOT_FOUND;
            continue;
        }
        fields[i].result = KII_JSON_FIELD_PARSE_SUCCESS;
        switch (value->type)
        {
            case JSMN_PRIMITIVE:
                fields[i].type = KII_JSON_FIELD_TYPE_PRIMITIVE;
                break;
            case JSMN_OBJECT:
                fields[i].type = KII_JSON_FIELD_TYPE_OBJECT;
                break;
            case JSMN_ARRAY:
                fields[i].type = KII_JSON_FIELD_TYPE_ARRAY;
                break;
            case JSMN_STRING:
                fields[i].type = KII_JSON_FIELD_TYPE_STRING;
                break;
            default:
                /* programming error */
                fields[i].result = KII_JSON_FIELD_PARSE_NOT_FOUND;
                continue;
        }
        fields[i].start = value->start;
        fields[i].end = value->end;
    }

    return KII_JSON_PARSE_SUCCESS;
}

kii_json_parse_result_t kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields)
{
    jsmn_parser parser;
    kii_json_parse_result_t ret = KII_JSON_PARSE_INVALID;
    int parse_result = JSMN_ERROR_NOMEM;
    jsmntok_t tokens[TOKEN_NUM];

    assert(json_string != NULL);

    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, json_string, json_string_len, tokens,
            TOKEN_NUM);
    if (parse_result > 0) {
        ret = prv_check_object_fields(json_string, json_string_len, tokens,
                fields);
    } else if (parse_result == 0) {
        ret = KII_JSON_PARSE_SUCCESS;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        M_KII_LOG(kii->kii_core.logger_cb(
            "Not enough tokens were provided\r\n"));
    } else if (parse_result == JSMN_ERROR_INVAL) {
        M_KII_LOG(kii->kii_core.logger_cb(
            "Invalid character inside JSON string\r\n"));
    } else if (parse_result == JSMN_ERROR_PART) {
        M_KII_LOG(kii->kii_core.logger_cb(
            "The string is not a full JSON packet, more bytes expected\r\n"));
    } else {
        M_KII_LOG(kii->kii_core.logger_cb(
            "Unexpected error: %d\r\n", parse_result));
    }

    return ret;
}

