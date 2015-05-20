#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jsmn.h>

#include "kii_utils.h"

static int prv_jsmn_token_num(const char* javascript, size_t javascript_len)
{
    jsmn_parser parser;

    assert(javascript != NULL);
    assert(javascript_len >= 0);

    jsmn_init(&parser);

    return jsmn_parse(&parser, javascript, javascript_len, NULL, 0);
}

int prv_kii_jsmn_get_tokens(
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

    len = prv_jsmn_token_num(javascript, javascript_len);
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

int prv_kii_jsmn_get_value(
        const char* javascript,
        size_t javascript_len,
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
                {
                    int num = prv_jsmn_token_num(
                            javascript + value_token->start,
                            javascript_len - value_token->start);
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

