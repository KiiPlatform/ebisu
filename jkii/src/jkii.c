#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <ctype.h>

#include <jsmn.h>

/* If your environment does not have assert, you must set JKII_NOASSERT define. */
#ifdef JKII_NOASSERT
  #define M_JKII_ASSERT(s)
#else
  #include <assert.h>
  #define M_JKII_ASSERT(s) assert(s)
#endif

#include <jkii.h>
#include <jkii_utils.h>

#define EVAL(f, v) f(v)
#define TOSTR(s) #s
#define LONG_MAX_STR EVAL(TOSTR, LONG_MAX)
#define LONGBUFSIZE (sizeof(LONG_MAX_STR) / sizeof(char) + 1)
#define INT_MAX_STR EVAL(TOSTR, INT_MAX)
#define INTBUFSIZE (sizeof(INT_MAX_STR) / sizeof(char) + 1)

/* "+ 3" denotes '-', '.', '\0' */
#define DOUBLEBUFSIZE DBL_MAX_10_EXP + 3

typedef enum _jkii_parent_type_t {
    _JKII_PARENT_TYPE_OBJECT,
    _JKII_PARENT_TYPE_ARRAY
} _jkii_parent_type_t;

typedef struct _jkii_target_t {
    union {
        const char* name;
        size_t index;
    } field;
    size_t len;
    _jkii_parent_type_t parent_type;
} _jkii_target_t;

static size_t _jkii_count_contained_token(const jsmntok_t* token)
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

static int _calculate_required_token_num(
    const char* json_string,
    size_t json_string_len)
{
    M_JKII_ASSERT(json_string != NULL);
    jsmn_parser parser;

    jsmn_init(&parser);
    int res = jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
    return res;
}

static jkii_parse_err_t _kii_jsmn_get_tokens(
        const char* json_string,
        size_t json_string_len,
        jkii_resource_t* resource)
{
    jsmn_parser parser;
    int parse_result = JSMN_ERROR_NOMEM;

    M_JKII_ASSERT(json_string != NULL);

    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, json_string, json_string_len,
        resource->tokens, resource->tokens_num);

    if (parse_result >= 0) {
        return JKII_ERR_OK;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        return JKII_ERR_TOKENS_SHORTAGE;
    } else if (parse_result == JSMN_ERROR_INVAL) {
        return JKII_ERR_INVALID_INPUT;
    } else if (parse_result == JSMN_ERROR_PART) {
        return JKII_ERR_INVALID_INPUT;
    } else {
        return JKII_ERR_INVALID_INPUT;
    }
}

static int _kii_jsmn_get_value(
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token)
{
    int i = 0;
    int index = 1;
    int ret = -1;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(tokens != NULL);
    M_JKII_ASSERT(name != NULL && strlen(name) > 0);
    M_JKII_ASSERT(out_token != NULL);

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
                index += _jkii_count_contained_token(value_token) + 1;
                break;
        }
    }

exit:
    return ret;
}

static int _jkii_is_all_digit(const char* buf, size_t buf_len)
{
    size_t i = 0;
    M_JKII_ASSERT(buf != NULL);

    for (i = 0; i < buf_len; ++i) {
        if (isdigit(buf[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static int _jkii_is_int(const char* buf, size_t buf_len)
{
    M_JKII_ASSERT(buf != NULL);
    if (buf_len > INTBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return _jkii_is_all_digit(buf, buf_len);
}

static int _jkii_is_long(const char* buf, size_t buf_len)
{
    M_JKII_ASSERT(buf != NULL);
    if (buf_len > LONGBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return _jkii_is_all_digit(buf, buf_len);
}

typedef enum  {
    parser_sts_sign,
    parser_sts_integer_start,
    parser_sts_integer0,
    parser_sts_integer,
    parser_sts_decimal,
    parser_sts_exp_start,
    parser_sts_exp
} num_parser_sts;

static int _jkii_is_double(const char* buf, size_t buf_len)
{
    M_JKII_ASSERT(buf != NULL);
    if (buf_len > DOUBLEBUFSIZE) {
        return 0;
    }

    num_parser_sts sts = parser_sts_sign;
    for (int i = 0; i < buf_len; ++i) {
        char c = buf[i];
        switch(sts) {
            case parser_sts_sign:
                if (c == '-') {
                    sts = parser_sts_integer_start;
                    continue;
                }
                if (c == '0') {
                    sts = parser_sts_integer0;
                    continue;
                }
                if ('1' <= c && c <= '9') {
                    sts = parser_sts_integer;
                    continue;
                }
                return 0;
            case parser_sts_integer_start:
                if (c == '0') {
                    sts = parser_sts_integer0;
                    continue;
                }
                if ('1' <= c && c <= '9') {
                    sts = parser_sts_integer;
                    continue;
                }
                return 0;
            case parser_sts_integer0:
                if (c == '.') {
                    sts = parser_sts_decimal;
                    continue;
                }
                if (c == 'e' || c == 'E') {
                    sts = parser_sts_exp_start;
                    continue;
                }
                return 0;
            case parser_sts_integer:
                if (isdigit(c)) {
                    continue;
                }
                if (c == '.') {
                    sts = parser_sts_decimal;
                    continue;
                }
                if (c == 'e' || c == 'E') {
                    sts = parser_sts_exp_start;
                    continue;
                }
                return 0;
            case parser_sts_decimal:
                if (isdigit(c)) {
                    continue;
                }
                if (c == 'e' || c == 'E') {
                    sts = parser_sts_exp_start;
                    continue;
                }
                return 0;
            case parser_sts_exp_start:
                if (c == '-' || c == '+' || isdigit(c)) {
                    sts = parser_sts_exp;
                    continue;
                }
                return 0;
            case parser_sts_exp:
                if (isdigit(c)) {
                    continue;
                }
                return 0;
            default:
                return 0;
        }
    }
    return 1;
}

static jkii_field_type_t _jkii_to_jkii_field_type(
        jsmntype_t jsmn_type,
        const char* buf,
        size_t buf_len)
{
    switch (jsmn_type)
    {
        case JSMN_PRIMITIVE:
            if (memcmp(buf, "null", buf_len) == 0) {
                return JKII_FIELD_TYPE_NULL;
            } else if (memcmp(buf, "true", buf_len) == 0 ||
                    memcmp(buf, "false", buf_len) == 0 ) {
                return JKII_FIELD_TYPE_BOOLEAN;
            } else if (_jkii_is_int(buf, buf_len) != 0) {
                return JKII_FIELD_TYPE_INTEGER;
            } else if (_jkii_is_long(buf, buf_len) != 0) {
                return JKII_FIELD_TYPE_LONG;
            } else if (_jkii_is_double(buf, buf_len) != 0) {
                return JKII_FIELD_TYPE_DOUBLE;
            } else {
                /* unexpected case. */
                return JKII_FIELD_TYPE_ANY;
            }
        case JSMN_OBJECT:
            return JKII_FIELD_TYPE_OBJECT;
        case JSMN_ARRAY:
            return JKII_FIELD_TYPE_ARRAY;
        case JSMN_STRING:
            return JKII_FIELD_TYPE_STRING;
        default:
            /* programming error */
            M_JKII_ASSERT(0);
            return JKII_FIELD_TYPE_ANY;
    }
}

static int _jkii_string_copy(
        const char* target,
        size_t target_size,
        char* out_buf,
        size_t out_buf_size)
{
    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_buf != NULL);

    if (out_buf_size <= target_size) {
        return -1;
    }
    memcpy(out_buf, target, target_size);
    out_buf[target_size] = '\0';

    return 0;
}

static jkii_primitive_err_t _jkii_to_long(
        const char* target,
        size_t target_size,
        long* out_long)
{
    char buf[LONGBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    long long_value = 0;

    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_long != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        if (*target == '-') {
            *out_long = LONG_MIN;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        }
        *out_long = LONG_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    long_value = strtol(buf, &endptr, 0);
    if (errno == ERANGE) {
        if (long_value == LONG_MAX) {
            *out_long = LONG_MAX;
            return JKII_PRIMITIVE_ERR_OVERFLOW;
        } else if (long_value == LONG_MIN) {
            *out_long = LONG_MIN;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        }
        M_JKII_ASSERT(0);
    } else if (errno == EINVAL) {
        /* This situation must not be occurred. This situation is */
        /* occurred when third argument of strtol is invalid. */
        M_JKII_ASSERT(0);
    }

    if (*endptr != '\0') {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

    *out_long = long_value;
    return JKII_PRIMITIVE_ERR_OK;
}

static jkii_primitive_err_t _jkii_to_int(
        const char* target,
        size_t target_size,
        int* out_int)
{
    long long_value = 0;

    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_int != NULL);

    if (_jkii_to_long(target, target_size, &long_value) ==
            JKII_PRIMITIVE_ERR_INVALID) {
        return JKII_PRIMITIVE_ERR_INVALID;
    } else if (long_value > INT_MAX) {
        *out_int = INT_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    } else if (long_value < INT_MIN) {
        *out_int = INT_MIN;
        return JKII_PRIMITIVE_ERR_UNDERFLOW;
    }

    *out_int = (int)long_value;
    return JKII_PRIMITIVE_ERR_OK;
}

static jkii_primitive_err_t _jkii_to_double(
        const char* target,
        size_t target_size,
        double* out_double)
{
    char buf[DOUBLEBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    double value = 0;

    M_JKII_ASSERT(buf != NULL);
    M_JKII_ASSERT(out_double != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "double string too long: %lu.", (unsigned long)target_size);
      return JKII_PRIMITIVE_ERR_INVALID;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    value = strtod(buf, &endptr);
    if (value == 0 && *endptr != '\0') {
        return JKII_PRIMITIVE_ERR_INVALID;
    } else if (errno == ERANGE) {
        if (value == 0) {
            *out_double = 0;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        } else {
            /* In this case, value is plus or minus HUGE_VAL. */
            *out_double = value;
            return JKII_PRIMITIVE_ERR_OVERFLOW;
        }
    }

    *out_double = value;
    return JKII_PRIMITIVE_ERR_OK;
}

static int _jkii_to_boolean(
        const char* buf,
        size_t buf_size,
        jkii_boolean_t* out_boolean)
{
    M_JKII_ASSERT(buf != NULL);
    M_JKII_ASSERT(out_boolean != NULL);

    if (memcmp(buf, "true", buf_size) == 0) {
        *out_boolean = JKII_TRUE;
        return 0;
    } else if (memcmp(buf, "false", buf_size) == 0) {
        *out_boolean = JKII_FALSE;
        return 0;
    }
    return -1;
}

static const char* _jkii_get_target(
        const char* path,
        _jkii_target_t* target)
{
    const char* start = NULL;
    const char* retval = NULL;
    const char* error = NULL;
    size_t path_len = strlen(path);
    size_t target_len = 0;
    int before_is_bash_slash = 0;

    M_JKII_ASSERT(path != NULL);
    M_JKII_ASSERT(target != NULL);

    if (path_len <= 1 || *path != '/' || strncmp(path, "//", 2) == 0) {
        error = path;
        retval = NULL;
        goto exit;
    }

    /* get length of target. */
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

    /* check contents. */
    if (*start == '[') {
        long value = 0;
        ++start;
        target_len -= 2;
        if (_jkii_is_long(start, target_len) == 0) {
            error = start;
            retval = NULL;
            goto exit;
        } else if (_jkii_to_long(start, target_len, &value)
                != JKII_PRIMITIVE_ERR_OK) {
            error = start;
            retval = NULL;
            goto exit;
        }
        target->field.index = (size_t)value;
        target->parent_type = _JKII_PARENT_TYPE_ARRAY;
    } else {
        target->field.name = start;
        target->len = target_len;
        target->parent_type = _JKII_PARENT_TYPE_OBJECT;
    }

exit:

    return retval;
}

static int _jkii_is_same_key(
        _jkii_target_t* target,
        const char* key,
        size_t key_len)
{
    size_t key_i = 0;
    size_t target_i = 0;
    int retval = 0;

    for (key_i = 0, target_i = 0;
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
        /* key does not reaches at the end of string. */
        return -1;
    } else if (target_i < target->len) {
        /* target does not reaches at the end of string. */
        return -1;
    }

    return retval;
}

static int _kii_jsmn_get_value_by_path(
        const char* json_string,
        size_t json_string_len,
        const jsmntok_t* tokens,
        const char* path,
        jsmntok_t** out_token)
{
    const char* next_root = path;
    const jsmntok_t* root_token = tokens;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(tokens != NULL);
    M_JKII_ASSERT(path != NULL && strlen(path) > 0);
    M_JKII_ASSERT(out_token != NULL);

    do {
        _jkii_target_t target;
        memset(&target, 0x00, sizeof(target));
        next_root = _jkii_get_target( next_root, &target);
        if (root_token->type == JSMN_OBJECT &&
                target.parent_type == _JKII_PARENT_TYPE_OBJECT) {
            size_t i = 0;
            size_t index = 1;
            const size_t root_token_size = (size_t)root_token->size;
            const jsmntok_t* next_token = NULL;
            for (i = 0; i < root_token_size; ++i) {
                const jsmntok_t* key_token = root_token + index;
                const jsmntok_t* value_token = root_token + index + 1;
                M_JKII_ASSERT(key_token->type == JSMN_STRING);
                if (_jkii_is_same_key(&target,
                                json_string + key_token->start,
                                key_token->end - key_token->start) == 0) {
                    next_token = value_token;
                    break;
                }
                index += _jkii_count_contained_token(value_token) + 1;
            }
            if (next_token == NULL) {
                return -1;
            }
            root_token = next_token;
        } else  if (root_token->type == JSMN_ARRAY &&
                target.parent_type == _JKII_PARENT_TYPE_ARRAY) {
            size_t i = 0;
            const size_t root_token_size = (size_t)root_token->size;
            if (target.field.index >= root_token_size) {
                return -1;
            }
            for (i = 0; i < target.field.index; ++i) {
                root_token += _jkii_count_contained_token(
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

static jkii_parse_err_t _jkii_convert_jsmntok_to_field(
        jkii_field_t* field,
        const jsmntok_t* value,
        const char* json_string,
        size_t json_string_len)
{
    jkii_parse_err_t retval = JKII_ERR_OK;
    jkii_field_type_t type = JKII_FIELD_TYPE_ANY;

    /* get actual type. */
    type = _jkii_to_jkii_field_type(value->type,
            json_string + value->start, value->end - value->start);
    if (type == JKII_FIELD_TYPE_ANY) {
        /* fail to get actual type. */
        return JKII_ERR_INVALID_INPUT;
    }

    if (field->type == JKII_FIELD_TYPE_ANY) {
        field->type = type;
    }

    field->start = value->start;
    field->end = value->end;

    switch (field->type) {
        case JKII_FIELD_TYPE_STRING:
        case JKII_FIELD_TYPE_OBJECT:
        case JKII_FIELD_TYPE_ARRAY:
            if (field->type != type) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else if (field->field_copy.string == NULL) {
                field->result = JKII_FIELD_ERR_OK;
            } else if (_jkii_string_copy(
                            json_string + value->start,
                            value->end - value->start,
                            field->field_copy.string,
                            field->field_copy_buff_size) == 0) {
                field->result = JKII_FIELD_ERR_OK;
            } else {
                field->result = JKII_FIELD_ERR_COPY;
                retval = JKII_ERR_PARTIAL;
            }
            break;
        case JKII_FIELD_TYPE_INTEGER:
            if (type != JKII_FIELD_TYPE_INTEGER) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else {
                jkii_primitive_err_t result =
                    _jkii_to_int(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.int_value));
                switch (result) {
                    case JKII_PRIMITIVE_ERR_OK:
                        field->result = JKII_FIELD_ERR_OK;
                        break;
                    case JKII_PRIMITIVE_ERR_OVERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_OVERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_UNDERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_INVALID:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        return JKII_ERR_INVALID_INPUT;
                }
            }
            break;
        case JKII_FIELD_TYPE_LONG:
            if (type != JKII_FIELD_TYPE_LONG &&
                    type != JKII_FIELD_TYPE_INTEGER) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else {
                jkii_primitive_err_t result =
                    _jkii_to_long(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.long_value));
                switch (result) {
                    case JKII_PRIMITIVE_ERR_OK:
                        field->result = JKII_FIELD_ERR_OK;
                        break;
                    case JKII_PRIMITIVE_ERR_OVERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_OVERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_UNDERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_INVALID:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        return JKII_ERR_INVALID_INPUT;
                }
            }
            break;
        case JKII_FIELD_TYPE_DOUBLE:
            if (type != JKII_FIELD_TYPE_DOUBLE) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else {
                jkii_primitive_err_t result =
                    _jkii_to_double(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.double_value));
                switch (result) {
                    case JKII_PRIMITIVE_ERR_OK:
                        field->result = JKII_FIELD_ERR_OK;
                        break;
                    case JKII_PRIMITIVE_ERR_OVERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_OVERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_UNDERFLOW:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        retval = JKII_ERR_PARTIAL;
                        break;
                    case JKII_PRIMITIVE_ERR_INVALID:
                        field->result = JKII_FIELD_ERR_NUM_UNDERFLOW;
                        return JKII_ERR_INVALID_INPUT;
                }
            }
            break;
        case JKII_FIELD_TYPE_BOOLEAN:
            if (type != JKII_FIELD_TYPE_BOOLEAN) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else if (_jkii_to_boolean(json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.boolean_value)) == 0) {
                field->result = JKII_FIELD_ERR_OK;
            } else {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                return JKII_ERR_INVALID_INPUT;
            }
            break;
        case JKII_FIELD_TYPE_NULL:
            if (type != JKII_FIELD_TYPE_NULL) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else if (memcmp(json_string + value->start, "null",
                            value->end - value->start) == 0) {
                field->result = JKII_FIELD_ERR_OK;
            } else {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                return JKII_ERR_INVALID_INPUT;
            }
            break;
        case JKII_FIELD_TYPE_ANY:
        default:
            return JKII_ERR_INVALID_INPUT;
    }

    return retval;
}

static jkii_parse_err_t _jkii_parse_fields(
    const char* json_string,
    size_t json_string_len,
    jkii_field_t* fields,
    jkii_resource_t* resource)
{
    jkii_parse_err_t ret = JKII_ERR_OK;
    for (jkii_field_t* field = fields; field->name != NULL || field->path != NULL; ++field) {
        int result = -1;
        jsmntok_t* value = NULL;

        /* get jsmntok_t pointing target value. */
        if (field->path != NULL) {
            result = _kii_jsmn_get_value_by_path(json_string,
                    json_string_len, resource->tokens, field->path,
                    &value);
        } else {
            result = _kii_jsmn_get_value(json_string, json_string_len,
                    resource->tokens, field->name, &value);
        }
        if (result != 0 || value == NULL)
        {
            ret = JKII_ERR_PARTIAL;
            field->result = JKII_FIELD_ERR_NOT_FOUND;
            continue;
        }

        /* convert jsmntok_t to jkii_field_t. */
        switch (_jkii_convert_jsmntok_to_field(field, value,
                        json_string, json_string_len)) {
            case JKII_ERR_OK:
                break;
            case JKII_ERR_PARTIAL:
                ret = JKII_ERR_PARTIAL;
                break;
            case JKII_ERR_INVALID_INPUT:
                return JKII_ERR_INVALID_INPUT;
            case JKII_ERR_ROOT_TYPE:
            default:
              /* unexpected error. */
              M_JKII_ASSERT(0);
              return JKII_ERR_INVALID_INPUT;
        }
    }
    return ret;
}

jkii_parse_err_t jkii_parse(
    const char* json_string,
    size_t json_string_len,
    jkii_field_t* fields,
    jkii_resource_t* resource)
{
    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(json_string_len > 0);

    if (resource == NULL || resource->tokens_num == 0) {
        return JKII_ERR_TOKENS_SHORTAGE;
    }

    jkii_parse_err_t res = JKII_ERR_INVALID_INPUT;
    res = _kii_jsmn_get_tokens(json_string, json_string_len, resource);
    if (res != JKII_ERR_OK) {
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        return JKII_ERR_INVALID_INPUT;
    }
    res = _jkii_parse_fields(json_string, json_string_len, fields, resource);

    return res;
}

jkii_parse_err_t jkii_parse_with_allocator(
    const char* json_string,
    size_t json_string_len,
    jkii_field_t* fields,
    JKII_CB_RESOURCE_ALLOC cb_alloc,
    JKII_CB_RESOURCE_FREE cb_free)
{
    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(json_string_len > 0);
    M_JKII_ASSERT(cb_alloc != NULL);
    M_JKII_ASSERT(cb_free != NULL);

    jkii_resource_t *resource;
    int required = _calculate_required_token_num(json_string, json_string_len);
    if (required > 0) {
        resource = cb_alloc(required);
        if (resource == NULL)
        {
            return JKII_ERR_ALLOCATION;
        }
    } else {
        return JKII_ERR_INVALID_INPUT;
    }

    jkii_parse_err_t res = _kii_jsmn_get_tokens(json_string, json_string_len, resource);
    if (res != JKII_ERR_OK) {
        cb_free(resource);
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        cb_free(resource);
        return JKII_ERR_INVALID_INPUT;
    }
    res = _jkii_parse_fields(json_string, json_string_len, fields, resource);

    cb_free(resource);
    return res;
}

jkii_primitive_err_t jkii_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    jkii_primitive_t* result)
{
    if (memcmp(primitive, "null", primitive_length) == 0)
    {
        result->type = JKII_FIELD_TYPE_NULL;
        return JKII_PRIMITIVE_ERR_OK;
    }
    if (memcmp(primitive, "true", primitive_length) == 0)
    {
        result->type = JKII_FIELD_TYPE_BOOLEAN;
        result->value.boolean_value = JKII_TRUE;
        return JKII_PRIMITIVE_ERR_OK;
    }
    if (memcmp(primitive, "false", primitive_length) == 0)
    {
        result->type = JKII_FIELD_TYPE_BOOLEAN;
        result->value.boolean_value = JKII_FALSE;
        return JKII_PRIMITIVE_ERR_OK;
    }
    int is_int = _jkii_is_int(primitive, primitive_length);
    if (is_int != 0) {
        int int_value = 0;
        jkii_primitive_err_t res = 
            _jkii_to_int(primitive, primitive_length, &int_value);
        if (res != JKII_PRIMITIVE_ERR_OK) {
            return res;
        }
        result->type = JKII_FIELD_TYPE_INTEGER;
        result->value.int_value = int_value;
        return JKII_PRIMITIVE_ERR_OK;
    }
    int is_long = _jkii_is_long(primitive, primitive_length);
    if (is_long != 0) {
        long long_value = 0;
        jkii_primitive_err_t res = 
            _jkii_to_long(primitive, primitive_length, &long_value);
        if (res != JKII_PRIMITIVE_ERR_OK) {
            return res;
        }
        result->type = JKII_FIELD_TYPE_LONG;
        result->value.long_value = long_value;
        return JKII_PRIMITIVE_ERR_OK;
    }
    int is_double = _jkii_is_double(primitive, primitive_length);
    if (is_double != 0) {
        double double_value = 0;
                jkii_primitive_err_t res = 
            _jkii_to_double(primitive, primitive_length, &double_value);
        if (res != JKII_PRIMITIVE_ERR_OK) {
            return res;
        }
        result->type = JKII_FIELD_TYPE_DOUBLE;
        result->value.double_value = double_value;
        return JKII_PRIMITIVE_ERR_OK;
    }
    return JKII_PRIMITIVE_ERR_INVALID;
}

int jkii_escape_str(const char* str, char* buff, size_t buff_size) {
    size_t str_len = strlen(str);
    if (buff_size < str_len + 1) {
        return -1;
    }
    size_t escaped_len = str_len;
    for (int i = 0, pos = 0; i < str_len; ++i, ++pos) {
        char c = str[i];
        // FIXME: Ignoriging \u{4 hex}
        if (c == '"' ||
            c == '/' ||
            c == '\\' ||
            c == '\b' ||
            c == '\f' ||
            c == '\n' ||
            c == '\r' ||
            c == '\t')
        {
            ++escaped_len;
            if (buff_size < escaped_len + 1) {
                return - 1;
            }
            buff[pos] = '\\';
            buff[++pos] = c;
        } else {
            buff[pos] = c;
        }
    }
    buff[escaped_len] = '\0';
    return escaped_len;
}