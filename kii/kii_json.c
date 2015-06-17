#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <errno.h>

#include <jsmn.h>

#include "kii_json.h"

#ifndef KII_JSON_TOKEN_NUM
/**
 * @def KII_JSON_TOKEN_NUM
 * @brief Json token size
 *
 * KII_JSON_TOKEN_NUM defines size of JSON can be parsed. By default
 * it is set to 128. If you've got error on JSON parsing in SDK, You
 * can increase the size of KII_JSON_TOKEN_NUM so that avoid error on
 * parsing large JSON. To change the size, please specify the size of
 * KII_JSON_TOKEN_NUM on build.
 */
#define KII_JSON_TOKEN_NUM 128
#endif

 // LONG_MAX size + 2. 2 means '\0' and '-'
#if LONG_MAX == 2147483647
    #define NUMBUF 12
#elif LONG_MAX == 9223372036854775807
  #define NUMBUF 21
#else
  #error LONG_MAX size is not expected.
#endif

typedef enum prv_kii_json_num_parse_result_t {
    PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS,
    PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW,
    PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW,
    PRV_KII_JSON_NUM_PARSE_RESULT_INVALID
} prv_kii_json_num_parse_result_t;

static void prv_kii_json_set_error_message(
        kii_json_t* kii_json,
        const char* message)
{
    strncpy(kii_json->error_string_buff, message,
            kii_json->error_string_length);
    kii_json->error_string_buff[kii_json->error_string_length - 1] = '\0';
}

static int prv_jsmn_token_num(const char* json_string, size_t json_string_len)
{
    jsmn_parser parser;

    assert(json_string != NULL);
    assert(json_string_len >= 0);

    jsmn_init(&parser);

    return jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
}

static int prv_kii_jsmn_get_tokens(
        kii_json_t* kii_json,
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
        prv_kii_json_set_error_message(kii_json,
                "Not enough tokens were provided");
    } else if (parse_result == JSMN_ERROR_INVAL) {
        ret = -1;
        prv_kii_json_set_error_message(kii_json,
                "Invalid character inside JSON string");
    } else if (parse_result == JSMN_ERROR_PART) {
        ret = -1;
        prv_kii_json_set_error_message(kii_json,
                "The string is not a full JSON packet, more bytes expected");
    } else {
        char buf[50];
        ret = -1;
        snprintf(buf, sizeof(buf) / sizeof(buf[0]),
                "Unexpected error: %d", parse_result);
        buf[31] = '\0';
        prv_kii_json_set_error_message(kii_json, buf);
    }

    return ret;
}

static int prv_kii_jsmn_get_value(
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
        int key_len = key_token->end - key_token->start;
        if (key_token->type != JSMN_STRING) {
            goto exit;
        }
        if (strlen(name) == key_len &&
                strncmp(name, json_string + key_token->start, key_len) == 0) {
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

static kii_json_field_type_t prv_kii_json_to_kii_json_field_type(
        jsmntype_t jsmn_type)
{
    switch (jsmn_type)
    {
        case JSMN_PRIMITIVE:
            return KII_JSON_FIELD_TYPE_PRIMITIVE;
        case JSMN_OBJECT:
            return KII_JSON_FIELD_TYPE_OBJECT;
        case JSMN_ARRAY:
            return KII_JSON_FIELD_TYPE_ARRAY;
        case JSMN_STRING:
            return KII_JSON_FIELD_TYPE_STRING;
        default:
            /* programming error */
            assert(0);
            return KII_JSON_FIELD_TYPE_PRIMITIVE;
    }
}

static int prv_kii_json_string_copy(
        kii_json_t* kii_json,
        const char* target,
        size_t target_size,
        char* out_buf,
        size_t out_buf_size)
{
    assert(target != NULL);
    assert(out_buf != NULL);

    if (out_buf_size <= target_size) {
        return -1;
    }
    memcpy(out_buf, target, target_size);
    out_buf[target_size] = '\0';

    return 0;
}

static prv_kii_json_num_parse_result_t prv_kii_json_to_long(
        kii_json_t* kii_json,
        const char* target,
        size_t target_size,
        long* out_long)
{
    char buf[NUMBUF];
    char* endptr = NULL;
    size_t buf_len = 0;
    long long_value = 0;

    assert(kii_json != NULL);
    assert(target != NULL);
    assert(out_long != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        // If checking more exactly, we should check contents of
        // target.
        if (*target == '-') {
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        } else {
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        }
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    long_value = strtol(buf, &endptr, 0);
    if (errno == ERANGE) {
        if (long_value == LONG_MAX) {
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        } else if (long_value == LONG_MIN) {
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        } else {
            prv_kii_json_set_error_message(kii_json,
                    "strtol set ERANGE but return is unexpected.");
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        }
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    }

    if (*endptr != '\0') {
        char message[50];
        snprintf(buf, sizeof(message) / sizeof(message[0]),
                "invalid long string: %s.", endptr);
        prv_kii_json_set_error_message(kii_json, message);
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    }

    *out_long = long_value;

    return PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static prv_kii_json_num_parse_result_t prv_kii_json_to_int(
        kii_json_t* kii_json,
        const char* target,
        size_t target_size,
        int* out_int)
{
    long long_value = 0;
    prv_kii_json_num_parse_result_t result =
        PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;

    assert(kii_json != NULL);
    assert(target != NULL);
    assert(out_int != NULL);

    result = prv_kii_json_to_long(kii_json, target, target_size, &long_value);
    if (result != PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS) {
        return result;
    } else if (long_value > INT_MAX) {
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    } else if (long_value < INT_MIN) {
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    }

    *out_int = (int)long_value;
    return PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static prv_kii_json_num_parse_result_t prv_kii_json_to_double(
        kii_json_t* kii_json,
        const char* target,
        size_t target_size,
        double* out_double)
{
    char buf[NUMBUF];
    char* endptr = NULL;
    size_t buf_len = 0;
    double value = 0;

    assert(kii_json != NULL);
    assert(buf != NULL);
    assert(out_double != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        // If checking more exactly, we should check contents of
        // target.
        if (*target == '-') {
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        } else {
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        }
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    value = strtod(buf, &endptr);
    if (value == 0 && *endptr != '\0') {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "invalid double string: %s.", endptr);
        prv_kii_json_set_error_message(kii_json, message);
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    } else if (value == 0 && errno == ERANGE) {
        return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
    } else if (value == HUGE_VAL && errno == ERANGE) {
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    }

    *out_double = value;
    return PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static int prv_kii_json_to_boolean(
        const char* buf,
        size_t buf_size,
        kii_json_boolean_t* out_boolean)
{
    assert(buf != NULL);
    assert(out_boolean != NULL);

    if (memcmp(buf, "true", buf_size) == 0) {
        *out_boolean = KII_JSON_TRUE;
        return 0;
    } else if (memcmp(buf, "false", buf_size) == 0) {
        *out_boolean = KII_JSON_FALSE;
        return 0;
    }
    return -1;
}

static kii_json_parse_result_t prv_kii_json_check_object_fields(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        kii_json_field_t* fields)
{
    int i;
    kii_json_parse_result_t retval = KII_JSON_PARSE_SUCCESS;

    switch (tokens[0].type)
    {
        case JSMN_ARRAY:
            return KII_JSON_PARSE_ROOT_TYPE_ERROR;
        case JSMN_OBJECT:
            break;
        default:
            return KII_JSON_PARSE_INVALID_INPUT;
    }

    for (i = 0; fields[i].name != NULL; ++i)
    {
        jsmntok_t* value = NULL;
        int result = 0;
        kii_json_field_t* field = &fields[i];
        kii_json_field_type_t type;

        result = prv_kii_jsmn_get_value(json_string, json_string_len, tokens,
                field->name, &value);
        if (result != 0 || value == NULL)
        {
            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            field->result = KII_JSON_FIELD_PARSE_NOT_FOUND;
            continue;
        }

        type = prv_kii_json_to_kii_json_field_type(value->type);
        if (field->type == KII_JSON_FIELD_TYPE_ANY) {
            field->type = type;
        }

        field->start = value->start;
        field->end = value->end;

        switch (field->type) {
            case KII_JSON_FIELD_TYPE_STRING:
            case KII_JSON_FIELD_TYPE_OBJECT:
            case KII_JSON_FIELD_TYPE_ARRAY:
            case KII_JSON_FIELD_TYPE_PRIMITIVE:
                if (field->type != type) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (field->field_copy.string == NULL) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else if (prv_kii_json_string_copy(kii_json,
                                json_string + value->start,
                                value->end - value->start,
                                field->field_copy.string,
                                field->field_copy_buff_size) == 0) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    field->result = KII_JSON_FIELD_PARSE_COPY_FAILED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                }
                break;
            case KII_JSON_FIELD_TYPE_INTEGER:
                if (type != KII_JSON_FIELD_TYPE_PRIMITIVE) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (field->field_copy.int_value == NULL) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    prv_kii_json_num_parse_result_t result =
                        prv_kii_json_to_int(kii_json,
                                json_string + value->start,
                                value->end - value->start,
                                field->field_copy.int_value);
                    switch (result) {
                        case PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                            field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_INVALID:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            return KII_JSON_PARSE_INVALID_INPUT;
                    }
                }
                break;
            case KII_JSON_FIELD_TYPE_LONG:
                if (type != KII_JSON_FIELD_TYPE_PRIMITIVE) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (field->field_copy.long_value == NULL) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    prv_kii_json_num_parse_result_t result =
                        prv_kii_json_to_long(kii_json,
                                json_string + value->start,
                                value->end - value->start,
                                field->field_copy.long_value);
                    switch (result) {
                        case PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                            field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_INVALID:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            return KII_JSON_PARSE_INVALID_INPUT;
                    }
                }
                break;
            case KII_JSON_FIELD_TYPE_DOUBLE:
                if (type != KII_JSON_FIELD_TYPE_PRIMITIVE) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (field->field_copy.double_value == NULL) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    prv_kii_json_num_parse_result_t result =
                        prv_kii_json_to_double(kii_json,
                                json_string + value->start,
                                value->end - value->start,
                                field->field_copy.double_value);
                    switch (result) {
                        case PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                            field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                            break;
                        case PRV_KII_JSON_NUM_PARSE_RESULT_INVALID:
                            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                            return KII_JSON_PARSE_INVALID_INPUT;
                    }
                }
                break;
            case KII_JSON_FIELD_TYPE_BOOLEAN:
                if (type != KII_JSON_FIELD_TYPE_PRIMITIVE) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (field->field_copy.boolean_value == NULL) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else if (prv_kii_json_to_boolean(json_string + value->start,
                                value->end - value->start,
                                field->field_copy.boolean_value) == 0) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    return KII_JSON_PARSE_INVALID_INPUT;
                }
                break;
            case KII_JSON_FIELD_TYPE_NULL:
                if (type != KII_JSON_FIELD_TYPE_PRIMITIVE) {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                } else if (memcmp(json_string + value->start, "null",
                                value->end - value->start) == 0) {
                    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                } else {
                    field->type = type;
                    field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                    return KII_JSON_PARSE_INVALID_INPUT;
                }
                break;
            case KII_JSON_FIELD_TYPE_ANY:
            default:
                prv_kii_json_set_error_message(kii_json,
                        "Unexpected kii_json_field_t.");
                return KII_JSON_PARSE_INVALID_INPUT;
        }
    }

    return retval;
}

kii_json_parse_result_t kii_json_read_object(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields)
{
    kii_json_parse_result_t ret = KII_JSON_PARSE_INVALID_INPUT;
    int result = -1;
    jsmntok_t tokens[KII_JSON_TOKEN_NUM];

    assert(json_string != NULL);
    assert(json_string_len > 0);

    result = prv_kii_jsmn_get_tokens(kii_json, json_string, json_string_len,
            tokens, sizeof(tokens) / sizeof(tokens[0]));
    if (result == 0)
    {
        ret = prv_kii_json_check_object_fields(kii_json, json_string,
                json_string_len, tokens, fields);
    }

    return ret;
}
