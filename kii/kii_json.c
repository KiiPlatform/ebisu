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

typedef enum prv_kii_json_convert {
    PRV_KII_JSON_CONVERT_SUCCESS,
    PRV_KII_JSON_CONVERT_EXPECTED_FAIL,
    PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL
} prv_kii_json_convert_t;

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

static prv_kii_json_convert_t prv_kii_json_jsmn_expected_type_to_kii_json_field(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        jsmntype_t expected_type,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);

    field->start = token->start;
    field->end = token->end;

    if (token->type != expected_type) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }

    if (field->field_copy.string != NULL) {
        size_t len = 0;
        len = token->end - token->start;
        if (field->field_copy_buff_size <= len) {
            field->result = KII_JSON_FIELD_PARSE_COPY_FAILED;
            return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
        }
        memcpy(field->field_copy.string, json_string + token->start, len);
        field->field_copy.string[len] = '\0';
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_long(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        long* out_value,
        kii_json_field_parse_result_t* out_parse_result)
{
    char buf[NUMBUF];
    char* endptr = NULL;
    size_t buf_len = 0;
    size_t actual_len = 0;

    assert(kii_json != NULL);
    assert(json_string != NULL);
    assert(out_value != NULL);
    assert(out_parse_result != NULL);

    actual_len = token->end - token->start;
    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= actual_len) {
        // If checking more exactly, we should check contents of
        // json_string.
        if (*(json_string + token->start) == '-') {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        } else {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
        }
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }
    memcpy(buf, json_string + token->start, actual_len);

    errno = 0;
    *out_value = strtol(buf, &endptr, 0);
    if (errno == ERANGE) {
        if (*out_value == LONG_MAX) {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
        } else if (*out_value == LONG_MIN) {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        } else {
            prv_kii_json_set_error_message(kii_json,
                    "strtol set ERANGE but return is unexpected.");
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        }
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }

    if (*endptr != '\0') {
        char message[50];
        snprintf(buf, sizeof(message) / sizeof(message[0]),
                "invalid long string: %s.", endptr);
        prv_kii_json_set_error_message(kii_json, message);
        *out_parse_result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
        return PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL;
    }

    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_kii_json_integer(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);
    assert(field->type == KII_JSON_FIELD_TYPE_INTEGER);

    field->start = token->start;
    field->end = token->end;

    if (token->type != JSMN_PRIMITIVE) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }


    if (field->field_copy.int_value != NULL) {
        long value = 0;
        prv_kii_json_convert_t conver_result =
            PRV_KII_JSON_CONVERT_EXPECTED_FAIL;

        conver_result = prv_kii_json_jsmn_primitive_to_long(kii_json, token,
                json_string, &value, &field->result);
        if (conver_result != PRV_KII_JSON_CONVERT_SUCCESS) {
            return conver_result;
        }

        if (value > INT_MAX) {
            field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
            return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
        } else if (value < INT_MIN) {
            field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
            return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
        }

        *(field->field_copy.int_value) = (int)value;
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_kii_json_long(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);
    assert(field->type == KII_JSON_FIELD_TYPE_LONG);

    field->start = token->start;
    field->end = token->end;

    if (token->type != JSMN_PRIMITIVE) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }


    if (field->field_copy.long_value != NULL) {
        long value = 0;
        prv_kii_json_convert_t conver_result =
            PRV_KII_JSON_CONVERT_EXPECTED_FAIL;

        conver_result = prv_kii_json_jsmn_primitive_to_long(kii_json, token,
                json_string, &value, &field->result);
        if (conver_result != PRV_KII_JSON_CONVERT_SUCCESS) {
            return conver_result;
        }

        *(field->field_copy.long_value) = (int)value;
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_double(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        double* out_value,
        kii_json_field_parse_result_t* out_parse_result)
{
    char buf[NUMBUF];
    char* endptr = NULL;
    size_t buf_len = 0;
    size_t actual_len = 0;
    double value = 0;

    assert(kii_json != NULL);
    assert(json_string != NULL);
    assert(out_value != NULL);
    assert(out_parse_result != NULL);

    actual_len = token->end - token->start;
    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= actual_len) {
        // If checking more exactly, we should check contents of
        // json_string.
        if (*(json_string + token->start) == '-') {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        } else {
            *out_parse_result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
        }
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }
    memcpy(buf, json_string + token->start, actual_len);

    errno = 0;
    value = strtod(buf, &endptr);
    if (value == 0 && *endptr != '\0') {
        char message[50];
        snprintf(buf, sizeof(message) / sizeof(message[0]),
                "invalid double string: %s.", endptr);
        prv_kii_json_set_error_message(kii_json, message);
        *out_parse_result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
        return PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL;
    } else if (value == 0 && errno == ERANGE) {
        *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    } else if (value == HUGE_VAL && errno == ERANGE) {
        *out_parse_result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }

    *out_value = value;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_kii_json_double(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);
    assert(field->type == KII_JSON_FIELD_TYPE_LONG);

    field->start = token->start;
    field->end = token->end;

    if (token->type != JSMN_PRIMITIVE) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }


    if (field->field_copy.double_value != NULL) {
        double value = 0;
        prv_kii_json_convert_t conver_result =
            PRV_KII_JSON_CONVERT_EXPECTED_FAIL;

        conver_result = prv_kii_json_jsmn_primitive_to_double(kii_json, token,
                json_string, &value, &field->result);
        if (conver_result != PRV_KII_JSON_CONVERT_SUCCESS) {
            return conver_result;
        }

        *(field->field_copy.double_value) = (int)value;
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_kii_json_boolean(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);
    assert(field->type == KII_JSON_FIELD_TYPE_LONG);

    field->start = token->start;
    field->end = token->end;

    if (token->type != JSMN_PRIMITIVE) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }


    if (field->field_copy.boolean_value != NULL) {
        const char* bool_str = NULL;
        size_t bool_str_len = 0;
        bool_str = json_string + token->start;
        bool_str_len = token->end - token->start;
        if (memcmp(bool_str, "true", bool_str_len) == 0) {
            *(field->field_copy.boolean_value) = KII_JSON_TRUE;
        } else if (memcmp(bool_str, "false", bool_str_len) == 0) {
            *(field->field_copy.boolean_value) = KII_JSON_FALSE;
        } else {
            field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
            return PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL;
        }
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
}

static prv_kii_json_convert_t prv_kii_json_jsmn_primitive_to_kii_json_null(
        kii_json_t* kii_json,
        const jsmntok_t* token,
        const char* json_string,
        kii_json_field_t* field)
{
    assert(token != NULL);
    assert(json_string != NULL);
    assert(field != NULL);
    assert(field->type == KII_JSON_FIELD_TYPE_LONG);

    field->start = token->start;
    field->end = token->end;

    if (token->type != JSMN_PRIMITIVE) {
        field->type = prv_kii_json_to_kii_json_field_type(token->type);
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_EXPECTED_FAIL;
    }


    if (memcmp(json_string + token->start, "null", token->end - token->start)
            != 0) {
        field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
        return PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL;
    }

    field->result = KII_JSON_FIELD_PARSE_SUCCESS;
    return PRV_KII_JSON_CONVERT_SUCCESS;
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
        prv_kii_json_convert_t convert_result = PRV_KII_JSON_CONVERT_SUCCESS;

        result = prv_kii_jsmn_get_value(json_string, json_string_len, tokens,
                field->name, &value);
        if (result != 0 || value == NULL)
        {
            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            field->result = KII_JSON_FIELD_PARSE_NOT_FOUND;
            continue;
        }

        if (field->type == KII_JSON_FIELD_TYPE_ANY) {
            field->type = prv_kii_json_to_kii_json_field_type(value->type);
        }

        switch (field->type) {
            case KII_JSON_FIELD_TYPE_STRING:
                convert_result =
                    prv_kii_json_jsmn_expected_type_to_kii_json_field(kii_json,
                            value, json_string, JSMN_STRING, field);
                break;
            case KII_JSON_FIELD_TYPE_OBJECT:
                convert_result =
                    prv_kii_json_jsmn_expected_type_to_kii_json_field(kii_json,
                            value, json_string, JSMN_OBJECT, field);
                break;
            case KII_JSON_FIELD_TYPE_ARRAY:
                convert_result =
                    prv_kii_json_jsmn_expected_type_to_kii_json_field(kii_json,
                            value, json_string, JSMN_ARRAY, field);
                break;
            case KII_JSON_FIELD_TYPE_PRIMITIVE:
                convert_result =
                    prv_kii_json_jsmn_expected_type_to_kii_json_field(kii_json,
                            value, json_string, JSMN_PRIMITIVE, field);
                break;
            case KII_JSON_FIELD_TYPE_INTEGER:
                convert_result =
                    prv_kii_json_jsmn_primitive_to_kii_json_integer(kii_json,
                            value, json_string, field);
                break;
            case KII_JSON_FIELD_TYPE_LONG:
                convert_result = prv_kii_json_jsmn_primitive_to_kii_json_long(
                        kii_json, value, json_string, field);
                break;
            case KII_JSON_FIELD_TYPE_DOUBLE:
                convert_result = prv_kii_json_jsmn_primitive_to_kii_json_double(
                        kii_json, value, json_string, field);
                break;
            case KII_JSON_FIELD_TYPE_BOOLEAN:
                convert_result =
                    prv_kii_json_jsmn_primitive_to_kii_json_boolean(kii_json,
                            value, json_string, field);
                break;
            case KII_JSON_FIELD_TYPE_NULL:
                convert_result = prv_kii_json_jsmn_primitive_to_kii_json_null(
                        kii_json, value, json_string, field);
                break;
            case KII_JSON_FIELD_TYPE_ANY:
            default:
                {
                    convert_result = PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL;
                    prv_kii_json_set_error_message(kii_json,
                            "Unexpected kii_json_field_t.");
                }
                break;
        }

        if (convert_result == PRV_KII_JSON_CONVERT_EXPECTED_FAIL) {
            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
        } else if (convert_result == PRV_KII_JSON_CONVERT_UNEXPECTED_FAIL) {
            // finish parsing.
            retval = KII_JSON_PARSE_INVALID_INPUT;
            break;
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
