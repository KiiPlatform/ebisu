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

#define EVAL(f, v) f(v)
#define TOSTR(s) #s
#define LONG_MAX_STR EVAL(TOSTR, LONG_MAX)
#define LONGBUFSIZE (sizeof(LONG_MAX_STR) / sizeof(char) + 1)
#define INT_MAX_STR EVAL(TOSTR, INT_MAX)
#define INTBUFSIZE (sizeof(INT_MAX_STR) / sizeof(char) + 1)

/* "+ 3" denotes '-', '.', '\0' */
#define DOUBLEBUFSIZE DBL_MAX_10_EXP + 3

typedef enum _kii_json_num_parse_result_t {
    _KII_JSON_NUM_PARSE_RESULT_SUCCESS,
    _KII_JSON_NUM_PARSE_RESULT_OVERFLOW,
    _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW,
    _KII_JSON_NUM_PARSE_RESULT_INVALID
} _kii_json_num_parse_result_t;

typedef enum _kii_json_parent_type_t {
    _KII_JSON_PARENT_TYPE_OBJECT,
    _KII_JSON_PARENT_TYPE_ARRAY
} _kii_json_parent_type_t;

typedef struct _kii_json_target_t {
    union {
        const char* name;
        size_t index;
    } field;
    size_t len;
    _kii_json_parent_type_t parent_type;
} _kii_json_target_t;

static size_t _kii_json_count_contained_token(const jsmntok_t* token)
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
    M_KII_JSON_ASSERT(json_string != NULL);
    jsmn_parser parser;

    jsmn_init(&parser);
    int res = jsmn_parse(&parser, json_string, json_string_len, NULL, 0);
    return res;
}

static kii_json_parse_result_t _kii_jsmn_get_tokens(
        const char* json_string,
        size_t json_string_len,
        kii_json_resource_t* resource)
{
    jsmn_parser parser;
    int parse_result = JSMN_ERROR_NOMEM;

    M_KII_JSON_ASSERT(json_string != NULL);

    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, json_string, json_string_len,
        resource->tokens, resource->tokens_num);

    if (parse_result >= 0) {
        return KII_JSON_PARSE_SUCCESS;
    } else if (parse_result == JSMN_ERROR_NOMEM) {
        return KII_JSON_PARSE_SHORTAGE_TOKENS;
    } else if (parse_result == JSMN_ERROR_INVAL) {
        return KII_JSON_PARSE_INVALID_INPUT;
    } else if (parse_result == JSMN_ERROR_PART) {
        return KII_JSON_PARSE_INVALID_INPUT;
    } else {
        return KII_JSON_PARSE_INVALID_INPUT;
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
                index += _kii_json_count_contained_token(value_token) + 1;
                break;
        }
    }

exit:
    return ret;
}

static int _kii_json_is_all_digit(const char* buf, size_t buf_len)
{
    size_t i = 0;
    M_KII_JSON_ASSERT(buf != NULL);

    for (i = 0; i < buf_len; ++i) {
        if (isdigit(buf[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

static int _kii_json_is_int(const char* buf, size_t buf_len)
{
    M_KII_JSON_ASSERT(buf != NULL);
    if (buf_len > INTBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return _kii_json_is_all_digit(buf, buf_len);
}

static int _kii_json_is_long(const char* buf, size_t buf_len)
{
    M_KII_JSON_ASSERT(buf != NULL);
    if (buf_len > LONGBUFSIZE) {
        return 0;
    }
    if (*buf == '-') {
        ++buf;
        --buf_len;
    }
    return _kii_json_is_all_digit(buf, buf_len);
}

static int _kii_json_is_double(const char* buf, size_t buf_len)
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
                    /* e must not be first. */
                    return 0;
                } else if (after_e != 0) {
                    /* e must not appear only once. */
                    return 0;
                } else if (before_is_minus != 0) {
                    /* e must follow digit. */
                    return 0;
                } else if (before_is_dot != 0) {
                    return 0;
                }
                before_is_e = 1;
                after_e = 1;
                break;
            case '.':
                if (is_first != 0) {
                    /* . must not be first. */
                    return 0;
                } else if (after_dot != 0) {
                    /* . must not appear only once. */
                    return 0;
                } else if (after_e != 0) {
                    /* . must not appear after e. */
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

static kii_json_field_type_t _kii_json_to_kii_json_field_type(
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
            } else if (_kii_json_is_int(buf, buf_len) != 0) {
                return KII_JSON_FIELD_TYPE_INTEGER;
            } else if (_kii_json_is_long(buf, buf_len) != 0) {
                return KII_JSON_FIELD_TYPE_LONG;
            } else if (_kii_json_is_double(buf, buf_len) != 0) {
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

static int _kii_json_string_copy(
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

static _kii_json_num_parse_result_t _kii_json_to_long(
        const char* target,
        size_t target_size,
        long* out_long)
{
    char buf[LONGBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    long long_value = 0;

    M_KII_JSON_ASSERT(target != NULL);
    M_KII_JSON_ASSERT(out_long != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        if (*target == '-') {
            *out_long = LONG_MIN;
            return _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        }
        *out_long = LONG_MAX;
        return _KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    long_value = strtol(buf, &endptr, 0);
    if (errno == ERANGE) {
        if (long_value == LONG_MAX) {
            *out_long = LONG_MAX;
            return _KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        } else if (long_value == LONG_MIN) {
            *out_long = LONG_MIN;
            return _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        }
        M_KII_JSON_ASSERT(0);
    } else if (errno == EINVAL) {
        /* This situation must not be occurred. This situation is */
        /* occurred when third argument of strtol is invalid. */
        M_KII_JSON_ASSERT(0);
    }

    if (*endptr != '\0') {
        return _KII_JSON_NUM_PARSE_RESULT_INVALID;
    }

    *out_long = long_value;
    return _KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static _kii_json_num_parse_result_t _kii_json_to_int(
        const char* target,
        size_t target_size,
        int* out_int)
{
    long long_value = 0;

    M_KII_JSON_ASSERT(target != NULL);
    M_KII_JSON_ASSERT(out_int != NULL);

    if (_kii_json_to_long(target, target_size, &long_value) ==
            _KII_JSON_NUM_PARSE_RESULT_INVALID) {
        return _KII_JSON_NUM_PARSE_RESULT_INVALID;
    } else if (long_value > INT_MAX) {
        *out_int = INT_MAX;
        return _KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
    } else if (long_value < INT_MIN) {
        *out_int = INT_MIN;
        return _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
    }

    *out_int = (int)long_value;
    return _KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static _kii_json_num_parse_result_t _kii_json_to_double(
        const char* target,
        size_t target_size,
        double* out_double)
{
    char buf[DOUBLEBUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    double value = 0;

    M_KII_JSON_ASSERT(buf != NULL);
    M_KII_JSON_ASSERT(out_double != NULL);

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        char message[50];
        snprintf(message, sizeof(message) / sizeof(message[0]),
                "double string too long: %lu.", (unsigned long)target_size);
      return _KII_JSON_NUM_PARSE_RESULT_INVALID;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    value = strtod(buf, &endptr);
    if (value == 0 && *endptr != '\0') {
        return _KII_JSON_NUM_PARSE_RESULT_INVALID;
    } else if (errno == ERANGE) {
        if (value == 0) {
            *out_double = 0;
            return _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW;
        } else {
            /* In this case, value is plus or minus HUGE_VAL. */
            *out_double = value;
            return _KII_JSON_NUM_PARSE_RESULT_OVERFLOW;
        }
    }

    *out_double = value;
    return _KII_JSON_NUM_PARSE_RESULT_SUCCESS;
}

static int _kii_json_to_boolean(
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

static const char* _kii_json_get_target(
        const char* path,
        _kii_json_target_t* target)
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
        if (_kii_json_is_long(start, target_len) == 0) {
            error = start;
            retval = NULL;
            goto exit;
        } else if (_kii_json_to_long(start, target_len, &value)
                != _KII_JSON_NUM_PARSE_RESULT_SUCCESS) {
            error = start;
            retval = NULL;
            goto exit;
        }
        target->field.index = (size_t)value;
        target->parent_type = _KII_JSON_PARENT_TYPE_ARRAY;
    } else {
        target->field.name = start;
        target->len = target_len;
        target->parent_type = _KII_JSON_PARENT_TYPE_OBJECT;
    }

exit:

    return retval;
}

static int _kii_json_is_same_key(
        _kii_json_target_t* target,
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

    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(tokens != NULL);
    M_KII_JSON_ASSERT(path != NULL && strlen(path) > 0);
    M_KII_JSON_ASSERT(out_token != NULL);

    do {
        _kii_json_target_t target;
        memset(&target, 0x00, sizeof(target));
        next_root = _kii_json_get_target( next_root, &target);
        if (root_token->type == JSMN_OBJECT &&
                target.parent_type == _KII_JSON_PARENT_TYPE_OBJECT) {
            size_t i = 0;
            size_t index = 1;
            const size_t root_token_size = (size_t)root_token->size;
            const jsmntok_t* next_token = NULL;
            for (i = 0; i < root_token_size; ++i) {
                const jsmntok_t* key_token = root_token + index;
                const jsmntok_t* value_token = root_token + index + 1;
                M_KII_JSON_ASSERT(key_token->type == JSMN_STRING);
                if (_kii_json_is_same_key(&target,
                                json_string + key_token->start,
                                key_token->end - key_token->start) == 0) {
                    next_token = value_token;
                    break;
                }
                index += _kii_json_count_contained_token(value_token) + 1;
            }
            if (next_token == NULL) {
                return -1;
            }
            root_token = next_token;
        } else  if (root_token->type == JSMN_ARRAY &&
                target.parent_type == _KII_JSON_PARENT_TYPE_ARRAY) {
            size_t i = 0;
            const size_t root_token_size = (size_t)root_token->size;
            if (target.field.index >= root_token_size) {
                return -1;
            }
            for (i = 0; i < target.field.index; ++i) {
                root_token += _kii_json_count_contained_token(
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

static kii_json_parse_result_t _kii_json_convert_jsmntok_to_field(
        kii_json_field_t* field,
        const jsmntok_t* value,
        const char* json_string,
        size_t json_string_len)
{
    kii_json_parse_result_t retval = KII_JSON_PARSE_SUCCESS;
    kii_json_field_type_t type = KII_JSON_FIELD_TYPE_ANY;

    /* get actual type. */
    type = _kii_json_to_kii_json_field_type(value->type,
            json_string + value->start, value->end - value->start);
    if (type == KII_JSON_FIELD_TYPE_ANY) {
        /* fail to get actual type. */
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
            } else if (_kii_json_string_copy(
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
                _kii_json_num_parse_result_t result =
                    _kii_json_to_int(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.int_value));
                switch (result) {
                    case _KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                        field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_INVALID:
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
                _kii_json_num_parse_result_t result =
                    _kii_json_to_long(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.long_value));
                switch (result) {
                    case _KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                        field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_INVALID:
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
                _kii_json_num_parse_result_t result =
                    _kii_json_to_double(
                            json_string + value->start,
                            value->end - value->start,
                            &(field->field_copy.double_value));
                switch (result) {
                    case _KII_JSON_NUM_PARSE_RESULT_SUCCESS:
                        field->result = KII_JSON_FIELD_PARSE_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_OVERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_OVERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_UNDERFLOW:
                        field->result = KII_JSON_FIELD_PARSE_COPY_UNDERFLOW;
                        retval = KII_JSON_PARSE_PARTIAL_SUCCESS;
                        break;
                    case _KII_JSON_NUM_PARSE_RESULT_INVALID:
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
            } else if (_kii_json_to_boolean(json_string + value->start,
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
            return KII_JSON_PARSE_INVALID_INPUT;
    }

    return retval;
}

static kii_json_parse_result_t _kii_json_parse_fields(
    const char* json_string,
    size_t json_string_len,
    kii_json_field_t* fields,
    kii_json_resource_t* resource)
{
    kii_json_parse_result_t ret = KII_JSON_PARSE_SUCCESS;
    for (kii_json_field_t* field = fields; field->name != NULL || field->path != NULL; ++field) {
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
            ret = KII_JSON_PARSE_PARTIAL_SUCCESS;
            field->result = KII_JSON_FIELD_PARSE_NOT_FOUND;
            continue;
        }

        /* convert jsmntok_t to kii_json_field_t. */
        switch (_kii_json_convert_jsmntok_to_field(field, value,
                        json_string, json_string_len)) {
            case KII_JSON_PARSE_SUCCESS:
                break;
            case KII_JSON_PARSE_PARTIAL_SUCCESS:
                ret = KII_JSON_PARSE_PARTIAL_SUCCESS;
                break;
            case KII_JSON_PARSE_INVALID_INPUT:
                return KII_JSON_PARSE_INVALID_INPUT;
            case KII_JSON_PARSE_ROOT_TYPE_ERROR:
            default:
              /* unexpected error. */
              M_KII_JSON_ASSERT(0);
              return KII_JSON_PARSE_INVALID_INPUT;
        }
    }
    return ret;
}

kii_json_parse_result_t kii_json_parse(
    const char* json_string,
    size_t json_string_len,
    kii_json_field_t* fields,
    kii_json_resource_t* resource)
{
    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(json_string_len > 0);

    if (resource == NULL || resource->tokens_num == 0) {
        return KII_JSON_PARSE_SHORTAGE_TOKENS;
    }

    kii_json_parse_result_t res = KII_JSON_PARSE_INVALID_INPUT;
    res = _kii_jsmn_get_tokens(json_string, json_string_len, resource);
    if (res != KII_JSON_PARSE_SUCCESS) {
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        return KII_JSON_PARSE_INVALID_INPUT;
    }
    res = _kii_json_parse_fields(json_string, json_string_len, fields, resource);

    return res;
}

kii_json_parse_result_t kii_json_parse_with_allocator(
    const char* json_string,
    size_t json_string_len,
    kii_json_field_t* fields,
    KII_JSON_RESOURCE_ALLOC_CB alloc_cb,
    KII_JSON_RESOURCE_FREE_CB free_cb)
{
    M_KII_JSON_ASSERT(json_string != NULL);
    M_KII_JSON_ASSERT(json_string_len > 0);
    M_KII_JSON_ASSERT(alloc_cb != NULL);
    M_KII_JSON_ASSERT(free_cb != NULL);

    kii_json_resource_t *resource;
    int required = _calculate_required_token_num(json_string, json_string_len);
    if (required > 0) {
        resource = alloc_cb(required);
        if (resource == NULL)
        {
            return KII_JSON_PARSE_ALLOCATION_ERROR;
        }
    } else {
        return KII_JSON_PARSE_INVALID_INPUT;
    }

    kii_json_parse_result_t res = _kii_jsmn_get_tokens(json_string, json_string_len, resource);
    if (res != KII_JSON_PARSE_SUCCESS) {
        free_cb(resource);
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        free_cb(resource);
        return KII_JSON_PARSE_INVALID_INPUT;
    }
    res = _kii_json_parse_fields(json_string, json_string_len, fields, resource);

    free_cb(resource);
    return res;
}
