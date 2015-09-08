#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <ctype.h>

#include <jsmn.h>

/* If your environment does not have assert, you must set KII_JSON_NOASSERT define. */
#ifdef KII_JSON_NOASSERT
  #define M_KII_JSON_ASSERT(s)
#else
  #include <assert.h>
  #define M_KII_JSON_ASSERT(s) assert(s)
#endif

#include <kii_json.h>

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

#define EVAL(f, v) f(v)
#define TOSTR(s) #s
#define LONG_MAX_STR EVAL(TOSTR, LONG_MAX)
#define LONGBUFSIZE (sizeof(LONG_MAX_STR) / sizeof(char) + 1)
#define INT_MAX_STR EVAL(TOSTR, INT_MAX)
#define INTBUFSIZE (sizeof(INT_MAX_STR) / sizeof(char) + 1)

/* "+ 3" denotes '-', '.', '\0' */
#define DOUBLEBUFSIZE DBL_MAX_10_EXP + 3

typedef enum prv_kii_json_num_parse_result_t {
    PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS,
    PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW,
    PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW,
    PRV_KII_JSON_NUM_PARSE_RESULT_INVALID
} prv_kii_json_num_parse_result_t;

typedef enum prv_kii_json_parent_type_t {
    PRV_KII_JSON_PARENT_TYPE_OBJECT,
    PRV_KII_JSON_PARENT_TYPE_ARRAY
} prv_kii_json_parent_type_t;

typedef struct prv_kii_json_target_t {
    union {
        const char* name;
        size_t index;
    } field;
    size_t len;
    prv_kii_json_parent_type_t parent_type;
} prv_kii_json_target_t;

static void prv_kii_json_set_error_message(
        kii_json_t* kii_json,
        const char* message)
{
    if (kii_json->error_string_buff == NULL) {
        return;
    }
    strncpy(kii_json->error_string_buff, message,
            kii_json->error_string_length);
    kii_json->error_string_buff[kii_json->error_string_length - 1] = '\0';
}

static size_t prv_kii_json_count_contained_token(const jsmntok_t* token)
{
    size_t retval = 0;
    int token_num = 1;
    do {
        ++retval;
        token_num += token->size;
        --token_num;
        ++token;
    } while (token_num > 0);
    return retval;
}

static int prv_kii_json_alloc_and_parse(
        kii_json_t *kii_json,
        const char* json_string,
        size_t json_string_len)
{
    jsmn_parser parser;
    int retval = 0;

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(json_string != NULL);

    jsmn_init(&parser);
    retval = jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
    if (retval < 0) {
        return retval;
    }
    if ((*kii_json->resource_cb)(kii_json->resource, retval) == 0) {
        return JSMN_ERROR_NOMEM;
    }

    jsmn_init(&parser);
    return jsmn_parse(&parser, json_string, json_string_len,
            kii_json->resource->tokens,
            kii_json->resource->tokens_num);
}

static kii_json_parse_result_t prv_kii_jsmn_get_tokens(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len)
{
    jsmn_parser parser;
    int parse_result = JSMN_ERROR_NOMEM;

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(json_string != NULL);

    if (kii_json->resource == NULL) {
        // This is application layer programming error.
        parse_result = JSMN_ERROR_NOMEM;
    } else if (kii_json->resource->tokens == NULL
            && kii_json->resource_cb == NULL) {
        // This is application layer programming error.
        parse_result = JSMN_ERROR_NOMEM;
    } else if (kii_json->resource->tokens != NULL &&
            kii_json->resource_cb == NULL) {
        jsmn_init(&parser);
        parse_result = jsmn_parse(&parser, json_string, json_string_len,
                kii_json->resource->tokens, kii_json->resource->tokens_num);
    } else if (kii_json->resource->tokens == NULL &&
            kii_json->resource_cb != NULL) {
        parse_result = prv_kii_json_alloc_and_parse(kii_json, json_string,
                json_string_len);
    } else {
        jsmn_init(&parser);
        parse_result = jsmn_parse(&parser, json_string, json_string_len,
                kii_json->resource->tokens, kii_json->resource->tokens_num);
        if (parse_result == JSMN_ERROR_NOMEM) {
            parse_result = prv_kii_json_alloc_and_parse(kii_json, json_string,
                    json_string_len);
        }
    }

    if (parse_result >= 0) {
        return KII_JSON_PARSE_SUCCESS;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        prv_kii_json_set_error_message(kii_json,
                "Not enough tokens were provided");
        return KII_JSON_PARSE_SHORTAGE_TOKENS;
    } else if (parse_result == JSMN_ERROR_INVAL) {
        prv_kii_json_set_error_message(kii_json,
                "Invalid character inside JSON string");
        return KII_JSON_PARSE_INVALID_INPUT;
    } else if (parse_result == JSMN_ERROR_PART) {
        prv_kii_json_set_error_message(kii_json,
                "The string is not a full JSON packet, more bytes expected");
        return KII_JSON_PARSE_INVALID_INPUT;
    } else {
        char buf[50];
        snprintf(buf, sizeof(buf) / sizeof(buf[0]),
                "Unexpected error: %d", parse_result);
        buf[31] = '\0';
        prv_kii_json_set_error_message(kii_json, buf);
        return KII_JSON_PARSE_INVALID_INPUT;
    }
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

    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(tokens != NULL);
    M_KII_JSON_ASSERT(name != NULL && strlen(name) > 0);
    M_KII_JSON_ASSERT(out_token != NULL);

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
                index += prv_kii_json_count_contained_token(value_token) + 1;
                break;
        }
    }

exit:
    return ret;
}

static int prv_kii_json_is_all_digit(const char* buf, size_t buf_len)
{
    int i = 0;
    M_KII_JSON_ASSERT(buf != NULL);

    for (i = 0; i < buf_len; ++i) {
        if (isdigit(buf[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static int prv_kii_json_is_int(const char* buf, size_t buf_len)
{
    M_KII_JSON_ASSERT(buf != NULL);
    if (buf_len > INTBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return prv_kii_json_is_all_digit(buf, buf_len);
}

static int prv_kii_json_is_long(const char* buf, size_t buf_len)
{
    M_KII_JSON_ASSERT(buf != NULL);
    if (buf_len > LONGBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return prv_kii_json_is_all_digit(buf, buf_len);
}

static int prv_kii_json_is_double(const char* buf, size_t buf_len)
{
    int is_first = 1;
    int after_e = 0;
    int before_is_e = 0;
    int after_dot = 0;
    int before_is_dot = 0;
    int before_is_minus = 0;
    size_t i = 0;

    M_KII_JSON_ASSERT(buf != NULL);
    if (buf_len > DOUBLEBUFSIZE) {
        return 0;
    }

    for (i = 0; i < buf_len; ++i) {
        switch(buf[i]) {
            case 'e':
                if (is_first != 0) {
                    // e must not be first.
                    return 0;
                } else if (after_e != 0) {
                    // e must not appear only once.
                    return 0;
                } else if (before_is_minus != 0) {
                    // e must follow digit.
                    return 0;
                } else if (before_is_dot != 0) {
                    return 0;
                }
                before_is_e = 1;
                after_e = 1;
                break;
            case '.':
                if (is_first != 0) {
                    // . must not be first.
                    return 0;
                } else if (after_dot != 0) {
                    // . must not appear only once.
                    return 0;
                } else if (after_e != 0) {
                    // . must not appear after e.
                    return 0;
                } else if (before_is_minus != 0) {
                    return 0;
                }
                before_is_dot = 1;
                after_dot = 1;
                break;
            case '-':
                if (is_first != 0) {
                    is_first = 0;
                    before_is_minus = 1;
                } else if (before_is_e != 0) {
                    before_is_minus = 1;
                } else {
                    return 0;
                }
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                is_first = 0;
                before_is_e = 0;
                before_is_dot = 0;
                before_is_minus = 0;
                break;
            default:
                return 0;
        }
    }
    return 1;
}

static kii_json_field_type_t prv_kii_json_to_kii_json_field_type(
        jsmntype_t jsmn_type,
        const char* buf,
        size_t buf_len)
{
    switch (jsmn_type)
    {
        case JSMN_PRIMITIVE:
            if (memcmp(buf, "null", buf_len) == 0) {
                return KII_JSON_FIELD_TYPE_NULL;
            } else if (memcmp(buf, "true", buf_len) == 0 ||
                    memcmp(buf, "false", buf_len) == 0 ) {
                return KII_JSON_FIELD_TYPE_BOOLEAN;
            } else if (prv_kii_json_is_int(buf, buf_len) != 0) {
                return KII_JSON_FIELD_TYPE_INTEGER;
            } else if (prv_kii_json_is_long(buf, buf_len) != 0) {
                return KII_JSON_FIELD_TYPE_LONG;
            } else if (prv_kii_json_is_double(buf, buf_len) != 0) {
                return KII_JSON_FIELD_TYPE_DOUBLE;
            } else {
                /* unexpected case. */
                return KII_JSON_FIELD_TYPE_ANY;
            }
        case JSMN_OBJECT:
            return KII_JSON_FIELD_TYPE_OBJECT;
        case JSMN_ARRAY:
            return KII_JSON_FIELD_TYPE_ARRAY;
        case JSMN_STRING:
            return KII_JSON_FIELD_TYPE_STRING;
        default:
            /* programming error */
            M_KII_JSON_ASSERT(0);
            return KII_JSON_FIELD_TYPE_ANY;
    }
}

static int prv_kii_json_string_copy(
        kii_json_t* kii_json,
        const char* target,
        size_t target_size,
        char* out_buf,
        size_t out_buf_size)
{
    M_KII_JSON_ASSERT(target != NULL);
    M_KII_JSON_ASSERT(out_buf != NULL);

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
    char buf[LONGBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    long long_value = 0;

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(target != NULL);
    M_KII_JSON_ASSERT(out_long != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        if (*target == '-') {
            *out_long = LONG_MIN;
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        }
        *out_long = LONG_MAX;
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    long_value = strtol(buf, &endptr, 0);
    if (errno == ERANGE) {
        if (long_value == LONG_MAX) {
            *out_long = LONG_MAX;
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        } else if (long_value == LONG_MIN) {
            *out_long = LONG_MIN;
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        }
        M_KII_JSON_ASSERT(0);
    } else if (errno == EINVAL) {
        // This situation must not be occurred. This situation is
        // occurred when third argument of strtol is invalid.
        M_KII_JSON_ASSERT(0);
    }

    if (*endptr != '\0') {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "invalid long string: %s.", endptr);
        message[49] = '\0';
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

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(target != NULL);
    M_KII_JSON_ASSERT(out_int != NULL);

    if (prv_kii_json_to_long(kii_json, target, target_size, &long_value) ==
            PRV_KII_JSON_NUM_PARSE_RESULT_INVALID) {
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    } else if (long_value > INT_MAX) {
        *out_int = INT_MAX;
        return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    } else if (long_value < INT_MIN) {
        *out_int = INT_MIN;
        return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
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
    char buf[DOUBLEBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    double value = 0;

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(buf != NULL);
    M_KII_JSON_ASSERT(out_double != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "double string too long: %lu.", target_size);
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
        return PRV_KII_JSON_NUM_PARSE_RESULT_INVALID;
    } else if (errno == ERANGE) {
        if (value == 0) {
            *out_double = 0;
            return PRV_KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        } else {
            /* In this case, value is plus or minus HUGE_VAL. */
            *out_double = value;
            return PRV_KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        }
    }

    *out_double = value;
    return PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static int prv_kii_json_to_boolean(
        const char* buf,
        size_t buf_size,
        kii_json_boolean_t* out_boolean)
{
    M_KII_JSON_ASSERT(buf != NULL);
    M_KII_JSON_ASSERT(out_boolean != NULL);

    if (memcmp(buf, "true", buf_size) == 0) {
        *out_boolean = KII_JSON_TRUE;
        return 0;
    } else if (memcmp(buf, "false", buf_size) == 0) {
        *out_boolean = KII_JSON_FALSE;
        return 0;
    }
    return -1;
}

static const char* prv_kii_json_get_target(
        kii_json_t* kii_json,
        const char* path,
        prv_kii_json_target_t* target)
{
    const char* start = NULL;
    const char* retval = NULL;
    const char* error = NULL;
    size_t path_len = strlen(path);
    size_t target_len = 0;
    int before_is_bash_slash = 0;

    M_KII_JSON_ASSERT(path != NULL);
    M_KII_JSON_ASSERT(target != NULL);

    if (path_len <= 1 || *path != '/' || strncmp(path, "//", 2) == 0) {
        error = path;
        retval = NULL;
        goto exit;
    }

    // get length of target.
    start = path + 1;
    for (before_is_bash_slash = 0, target_len = 0, retval = start;
            *retval != '\0'; ++retval) {
        if (*retval == '/' && before_is_bash_slash == 0) {
            break;
        } else if (*retval == '\\') {
            before_is_bash_slash = 1;
        } else {
            before_is_bash_slash = 0;
        }
        ++target_len;

    }
    if (*retval == '\0') {
        retval = NULL;
    }

    // check contents.
    if (*start == '[') {
        long value = 0;
        ++start;
        target_len -= 2;
        if (prv_kii_json_is_long(start, target_len) == 0) {
            error = start;
            retval = NULL;
            goto exit;
        } else if (prv_kii_json_to_long(kii_json, start, target_len, &value)
                != PRV_KII_JSON_NUM_PARSE_RESULT_SUCCESS) {
            error = start;
            retval = NULL;
            goto exit;
        }
        target->field.index = (size_t)value;
        target->parent_type = PRV_KII_JSON_PARENT_TYPE_ARRAY;
    } else {
        target->field.name = start;
        target->len = target_len;
        target->parent_type = PRV_KII_JSON_PARENT_TYPE_OBJECT;
    }

exit:
    if (error != NULL) {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "Invalid path: %s.", error);
        message[49] = '\0';
        prv_kii_json_set_error_message(kii_json, message);
    }
    return retval;
}

static int prv_kii_json_is_same_key(
        prv_kii_json_target_t* target,
        const char* key,
        size_t key_len)
{
    int key_i = 0;
    int target_i = 0;
    int retval = 0;

    for (key_i =0, target_i = 0;
            key_i < key_len && target_i < target->len; ++key_i) {
        char key_c = key[key_i];
        char target_c1 = target->field.name[target_i];
        char target_c2 = target->field.name[target_i + 1];
        if (key_c == target_c1) {
            ++target_i;
        } else if (key_c == '/' && (target_c1 == '\\' && target_c2 == '/')) {
            target_i += 2;
        } else if (key_c == '\\' && (target_c1 == '\\' && target_c2 == '\\')) {
            target_i += 2;
        } else if (key_c == '[' && (target_c1 == '\\' && target_c2 == '[')) {
            target_i += 2;
        } else if (key_c == ']' && (target_c1 == '\\' && target_c2 == ']')) {
            target_i += 2;
        } else {
            retval = -1;
            break;
        }
    }

    if (key_i < key_len) {
        // key does not reaches at the end of string.
        return -1;
    } else if (target_i < target->len) {
        // target does not reaches at the end of string.
        return -1;
    }

    return retval;
}

static int prv_kii_jsmn_get_value_by_path(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        const char* path,
        jsmntok_t** out_token)
{
    const char* next_root = path;
    const jsmntok_t* root_token = tokens;

    M_KII_JSON_ASSERT(kii_json != NULL);
    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(tokens != NULL);
    M_KII_JSON_ASSERT(path != NULL && strlen(path) > 0);
    M_KII_JSON_ASSERT(out_token != NULL);

    do {
        prv_kii_json_target_t target;
        memset(&target, 0x00, sizeof(target));
        next_root = prv_kii_json_get_target(kii_json, next_root, &target);
        if (root_token->type == JSMN_OBJECT &&
                target.parent_type == PRV_KII_JSON_PARENT_TYPE_OBJECT) {
            size_t i = 0;
            size_t index = 1;
            const jsmntok_t* next_token = NULL;
            for (i = 0; i < root_token->size; ++i) {
                const jsmntok_t* key_token = root_token + index;
                const jsmntok_t* value_token = root_token + index + 1;
                M_KII_JSON_ASSERT(key_token->type == JSMN_STRING);
                if (prv_kii_json_is_same_key(&target,
                                json_string + key_token->start,
                                key_token->end - key_token->start) == 0) {
                    next_token = value_token;
                    break;
                }
                index += prv_kii_json_count_contained_token(value_token) + 1;
            }
            if (next_token == NULL) {
                return -1;
            }
            root_token = next_token;
        } else  if (root_token->type == JSMN_ARRAY &&
                target.parent_type == PRV_KII_JSON_PARENT_TYPE_ARRAY) {
            size_t i = 0;
            if (target.field.index >= root_token->size) {
                return -1;
            }
            for (i = 0; i < target.field.index; ++i) {
                root_token += prv_kii_json_count_contained_token(
                        root_token + 1);
            }
            ++root_token;
        } else {
            return -1;
        }
    } while (next_root != NULL);

    *out_token = (jsmntok_t*)root_token;
    return 0;
}

static kii_json_parse_result_t prv_kii_json_convert_jsmntok_to_field(
        kii_json_t* kii_json,
        kii_json_field_t* field,
        const jsmntok_t* value,
        const char* json_string,
        size_t json_string_len)
{
    kii_json_parse_result_t retval = KII_JSON_PARSE_SUCCESS;
    kii_json_field_type_t type = KII_JSON_FIELD_TYPE_ANY;

    // get actual type.
    type = prv_kii_json_to_kii_json_field_type(value->type,
            json_string + value->start, value->end - value->start);
    if (type == KII_JSON_FIELD_TYPE_ANY) {
        // fail to get actual type.
        return KII_JSON_PARSE_INVALID_INPUT;
    }

    if (field->type == KII_JSON_FIELD_TYPE_ANY) {
        field->type = type;
    }

    field->start = value->start;
    field->end = value->end;

    switch (field->type) {
        case KII_JSON_FIELD_TYPE_STRING:
        case KII_JSON_FIELD_TYPE_OBJECT:
        case KII_JSON_FIELD_TYPE_ARRAY:
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
            if (type != KII_JSON_FIELD_TYPE_INTEGER) {
                field->type = type;
                field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            } else {
                prv_kii_json_num_parse_result_t result =
                    prv_kii_json_to_int(kii_json,
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.int_value));
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
            if (type != KII_JSON_FIELD_TYPE_LONG &&
                    type != KII_JSON_FIELD_TYPE_INTEGER) {
                field->type = type;
                field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            } else {
                prv_kii_json_num_parse_result_t result =
                    prv_kii_json_to_long(kii_json,
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.long_value));
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
            if (type != KII_JSON_FIELD_TYPE_DOUBLE) {
                field->type = type;
                field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            } else {
                prv_kii_json_num_parse_result_t result =
                    prv_kii_json_to_double(kii_json,
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.double_value));
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
            if (type != KII_JSON_FIELD_TYPE_BOOLEAN) {
                field->type = type;
                field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            } else if (prv_kii_json_to_boolean(json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.boolean_value)) == 0) {
                field->result = KII_JSON_FIELD_PARSE_SUCCESS;
            } else {
                field->type = type;
                field->result = KII_JSON_FIELD_PARSE_TYPE_UNMATCHED;
                return KII_JSON_PARSE_INVALID_INPUT;
            }
            break;
        case KII_JSON_FIELD_TYPE_NULL:
            if (type != KII_JSON_FIELD_TYPE_NULL) {
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

    return retval;
}

static kii_json_parse_result_t prv_kii_json_read_object(
        kii_json_t* kii_json,
        const char* json_string,
        size_t json_string_len,
        kii_json_field_t* fields)
{
    kii_json_parse_result_t retval = KII_JSON_PARSE_INVALID_INPUT;
    kii_json_field_t* field = NULL;

    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(json_string_len > 0);

    retval = prv_kii_jsmn_get_tokens(kii_json, json_string, json_string_len);
    if (retval != KII_JSON_PARSE_SUCCESS) {
        return retval;
    }

    switch (kii_json->resource->tokens[0].type) {
        case JSMN_ARRAY:
        case JSMN_OBJECT:
            break;
        default:
            return KII_JSON_PARSE_INVALID_INPUT;
    }

    for (field = fields; field->name != NULL || field->path != NULL; ++field) {
        int result = -1;
        jsmntok_t* value = NULL;

        // get jsmntok_t pointing target value.
        if (field->path != NULL) {
            result = prv_kii_jsmn_get_value_by_path(kii_json, json_string,
                    json_string_len, kii_json->resource->tokens, field->path,
                    &value);
        } else {
            result = prv_kii_jsmn_get_value(json_string, json_string_len,
                    kii_json->resource->tokens, field->name, &value);
        }
        if (result != 0 || value == NULL)
        {
            retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
            field->result = KII_JSON_FIELD_PARSE_NOT_FOUND;
            continue;
        }

        // convert jsmntok_t to kii_json_field_t.
        switch (prv_kii_json_convert_jsmntok_to_field(kii_json, field, value,
                        json_string, json_string_len)) {
            case KII_JSON_PARSE_SUCCESS:
                break;
            case KII_JSON_PARSE_PARTIAL_SUCCESS:
                retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                break;
            case KII_JSON_PARSE_INVALID_INPUT:
                return KII_JSON_PARSE_INVALID_INPUT;
            case KII_JSON_PARSE_ROOT_TYPE_ERROR:
            default:
              // unexpected error.
              M_KII_JSON_ASSERT(0);
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
#ifdef KII_JSON_FIXED_TOKEN_NUM
    {
        kii_json_parse_result_t retval;
        kii_json_token_t tokens[KII_JSON_FIXED_TOKEN_NUM];
        kii_json_resource_t* original_resource = kii_json->resource;
        kii_json_resource_t stacked_resource;
        KII_JSON_RESOURCE_CB resource_cb = kii_json->resource_cb;

        stacked_resource.tokens = tokens;
        stacked_resource.tokens_num = sizeof(tokens) / sizeof(tokens[0]);
        kii_json->resource = &stacked_resource;
        kii_json->resource_cb = NULL;

        retval = prv_kii_json_read_object(kii_json, json_string,
                json_string_len, fields);

        kii_json->resource = original_resource;
        kii_json->resource_cb = resource_cb;
        return retval;
    }
#else
    return prv_kii_json_read_object(kii_json, json_string, json_string_len,
            fields);
#endif
}
