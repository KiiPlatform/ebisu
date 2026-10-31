#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <ctype.h>

/* Some vendor libcs have an errno.h that declares neither errno nor the codes
 * jkii checks. Defined here, once, for the file that uses them: in a public
 * header every file including it defined errno again, and got EINVAL and
 * ERANGE whether it wanted them or not. */
#ifdef ERRNO_HACK_NEEDED
int errno;
#ifndef ERANGE
#define ERANGE 34
#endif
#endif

/* If your environment does not have assert, you must set JKII_NOASSERT define. */
#ifdef JKII_NOASSERT
  #define M_JKII_ASSERT(s)
#else
  #include <assert.h>
  #define M_JKII_ASSERT(s) assert(s)
#endif

#include <jkii.h>
#include <jkii_utils.h>

/* Compares a counted, non-terminated buffer against a string literal.
 *
 * The obvious memcmp(buf, "null", buf_len) is wrong in both directions. When
 * buf_len is longer than the literal it reads off the end of the literal --
 * parsing the primitive 2147483647 reads ten bytes out of five. When buf_len is
 * shorter it compares only a prefix, so a one-character primitive "n" matches
 * "null" and is misclassified. Length has to be part of the comparison.
 *
 * literal must be a string literal, so that sizeof gives its length. */
#define M_JKII_EQ_LITERAL(buf, buf_len, literal) \
    ((buf_len) == sizeof(literal) - 1 && \
     memcmp((buf), (literal), sizeof(literal) - 1) == 0)
/* The longest an int64_t is in decimal, INT64_MIN, written out: how a
 * limits.h macro is spelled is up to the libc, and glibc's are hexadecimal. */
#define INT64_MIN_STR "-9223372036854775808"
/* With the terminator. An integer longer than that is out of range, since it
 * may not have leading zeros. */
#define INT64BUFSIZE sizeof(INT64_MIN_STR)

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
        jkii_resource_t* resource,
        size_t* out_tokens_num)
{
    jsmn_parser parser;
    int parse_result = JSMN_ERROR_NOMEM;

    M_JKII_ASSERT(json_string != NULL);

    jsmn_init(&parser);
    parse_result = jsmn_parse(&parser, json_string, json_string_len,
        resource->tokens, resource->tokens_num);

    if (parse_result > 0) {
        if (out_tokens_num != NULL) {
            *out_tokens_num = (size_t)parse_result;
        }
        return JKII_ERR_OK;
    } else if (parse_result == 0) {
        /* Empty, whitespace only, or a NUL before the first token. That can
         * come from the network, so it is invalid input rather than something
         * to assert against. jsmn has left the tokens of the previous parse in
         * place, which the caller would otherwise go on to read. */
        return JKII_ERR_INVALID_INPUT;
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
        size_t tokens_num,
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

    if (tokens[0].type != JSMN_OBJECT) {
        goto exit;
    }

    /* jsmn is not strict, so a key need not have a value: {"a"} is an
     * object holding a single string. Look no further than the tokens this
     * parse produced; past them lie an earlier parse's tokens, or none. */
    for (i = 0; i < tokens[0].size && (size_t)index + 1 < tokens_num; ++i) {
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
            case JSMN_UNDEFINED:
                goto exit;
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
        if (isdigit((unsigned char)buf[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

/* Whether buf is written as an integer: digits, after an optional minus sign.
 * Whether it is an int32_t or an int64_t depends on its value, which this does
 * not look at: see _jkii_to_integer. */
static int _jkii_is_integer(const char* buf, size_t buf_len)
{
    M_JKII_ASSERT(buf != NULL);
    if (buf_len > 0 && *buf == '-') {
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
                if (isdigit((unsigned char)c)) {
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
                if (isdigit((unsigned char)c)) {
                    continue;
                }
                if (c == 'e' || c == 'E') {
                    sts = parser_sts_exp_start;
                    continue;
                }
                return 0;
            case parser_sts_exp_start:
                if (c == '-' || c == '+' || isdigit((unsigned char)c)) {
                    sts = parser_sts_exp;
                    continue;
                }
                return 0;
            case parser_sts_exp:
                if (isdigit((unsigned char)c)) {
                    continue;
                }
                return 0;
            default:
                return 0;
        }
    }
    return 1;
}

static jkii_primitive_err_t _jkii_to_int64(
        const char* target,
        size_t target_size,
        int64_t* out_int64);

/* An integer's type follows its value: JKII_FIELD_TYPE_INTEGER if it fits in
 * an int32_t, JKII_FIELD_TYPE_LONG if not. One that could not be read at all is
 * JKII_FIELD_TYPE_INTEGER, so that converting it as the field asks reports
 * it. */
static jkii_field_type_t _jkii_integer_type(
        int64_t value,
        jkii_primitive_err_t res)
{
    if (res == JKII_PRIMITIVE_ERR_INVALID ||
            (res == JKII_PRIMITIVE_ERR_OK &&
             INT32_MIN <= value && value <= INT32_MAX)) {
        return JKII_FIELD_TYPE_INTEGER;
    }
    return JKII_FIELD_TYPE_LONG;
}

/* An integer's type depends on its value, so finding it means converting the
 * integer: the result is handed back in out_integer and out_integer_res, for
 * the field to take rather than convert it a second time. */
static jkii_field_type_t _jkii_to_jkii_field_type(
        jsmntype_t jsmn_type,
        const char* buf,
        size_t buf_len,
        int64_t* out_integer,
        jkii_primitive_err_t* out_integer_res)
{
    switch (jsmn_type)
    {
        case JSMN_PRIMITIVE:
            if (M_JKII_EQ_LITERAL(buf, buf_len, "null")) {
                return JKII_FIELD_TYPE_NULL;
            } else if (M_JKII_EQ_LITERAL(buf, buf_len, "true") ||
                    M_JKII_EQ_LITERAL(buf, buf_len, "false")) {
                return JKII_FIELD_TYPE_BOOLEAN;
            } else if (_jkii_is_integer(buf, buf_len) != 0) {
                *out_integer_res = _jkii_to_int64(buf, buf_len, out_integer);
                return _jkii_integer_type(*out_integer, *out_integer_res);
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

static jkii_primitive_err_t _jkii_to_int64(
        const char* target,
        size_t target_size,
        int64_t* out_int64)
{
    char buf[INT64BUFSIZE];
    char* endptr = NULL;
    size_t buf_len = 0;
    long long value = 0;
    const char* digits = NULL;
    size_t digits_len = 0;

    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_int64 != NULL);

    /* A JSON number has no leading zeros: 0 and -0 are numbers, 00, 007 and
     * 010 are not. Checked before the length, so that a long run of zeros is
     * not taken for an overflow. */
    digits = target;
    digits_len = target_size;
    if (digits_len > 0 && *digits == '-') {
        ++digits;
        --digits_len;
    }
    if (digits_len > 1 && digits[0] == '0') {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

    buf_len = sizeof(buf) / sizeof(buf[0]);
    memset(buf, 0, sizeof(buf) / sizeof(buf[0]));

    if (buf_len <= target_size) {
        if (*target == '-') {
            *out_int64 = INT64_MIN;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        }
        *out_int64 = INT64_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    }
    memcpy(buf, target, target_size);

    errno = 0;
    /* JSON numbers are decimal. Base 0 would read 0x and a leading 0 as
     * hexadecimal and octal, which the checks above leave strtoll no chance to
     * do, but base 10 says so here. long long has at least 64 bits, so every
     * int64_t is in its range, whatever the width of long. */
    value = strtoll(buf, &endptr, 10);
    if (endptr == buf) {
        /* No digits, e.g. "-" or "". Whether strtoll says so in errno is up
         * to the libc: BSD and macOS set EINVAL, glibc and newlib do not.
         * Every libc leaves endptr at buf, so go by that. */
        return JKII_PRIMITIVE_ERR_INVALID;
    } else if (errno == ERANGE) {
        /* Past the long long range: strtoll gives LLONG_MAX or LLONG_MIN, so
         * the sign tells which. */
        if (value < 0) {
            *out_int64 = INT64_MIN;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        }
        *out_int64 = INT64_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    }

    if (*endptr != '\0') {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

#if LLONG_MAX > INT64_MAX
    /* Within the long long range but not the int64_t one. */
    if (value > INT64_MAX) {
        *out_int64 = INT64_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    } else if (value < INT64_MIN) {
        *out_int64 = INT64_MIN;
        return JKII_PRIMITIVE_ERR_UNDERFLOW;
    }
#endif

    *out_int64 = (int64_t)value;
    return JKII_PRIMITIVE_ERR_OK;
}

/* Narrows the result of _jkii_to_int64() to an int32_t, saturating where it
 * does not fit. */
static jkii_primitive_err_t _jkii_narrow_to_int32(
        int64_t value,
        jkii_primitive_err_t res,
        int32_t* out_int32)
{
    M_JKII_ASSERT(out_int32 != NULL);

    if (res == JKII_PRIMITIVE_ERR_INVALID) {
        return JKII_PRIMITIVE_ERR_INVALID;
    } else if (res == JKII_PRIMITIVE_ERR_OVERFLOW || value > INT32_MAX) {
        *out_int32 = INT32_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    } else if (res == JKII_PRIMITIVE_ERR_UNDERFLOW || value < INT32_MIN) {
        *out_int32 = INT32_MIN;
        return JKII_PRIMITIVE_ERR_UNDERFLOW;
    }

    *out_int32 = (int32_t)value;
    return JKII_PRIMITIVE_ERR_OK;
}

/* Reads an integer as an int32_t if it fits in one and as an int64_t if not,
 * so that the type follows the value rather than how many characters it takes
 * or how wide int and long are. One that does not fit in an int64_t either is
 * an int64_t that overflowed or underflowed, and out_integer then holds
 * INT64_MAX or INT64_MIN. */
static jkii_primitive_err_t _jkii_to_integer(
        const char* target,
        size_t target_size,
        jkii_primitive_t* out_integer)
{
    int64_t value = 0;
    jkii_primitive_err_t res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_integer != NULL);

    res = _jkii_to_int64(target, target_size, &value);
    if (res == JKII_PRIMITIVE_ERR_INVALID) {
        return res;
    }
    out_integer->type = _jkii_integer_type(value, res);
    if (out_integer->type == JKII_FIELD_TYPE_INTEGER) {
        out_integer->value.int_value = (int32_t)value;
    } else {
        out_integer->type = JKII_FIELD_TYPE_LONG;
        out_integer->value.long_value = value;
    }
    return res;
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
    if (endptr == buf || *endptr != '\0') {
        /* Nothing converted, or not all of it: strtod reads "1e" as 1 and
         * stops at the "e". */
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

    if (M_JKII_EQ_LITERAL(buf, buf_size, "true")) {
        *out_boolean = JKII_TRUE;
        return 0;
    } else if (M_JKII_EQ_LITERAL(buf, buf_size, "false")) {
        *out_boolean = JKII_FALSE;
        return 0;
    }
    return -1;
}

/* An array index read from a path as a size_t. One that is negative or past
 * the size_t range is SIZE_MAX, which no array reaches, since jsmn counts
 * elements in an int. A cast alone makes -1 SIZE_MAX as well, but where size_t
 * has 32 bits it would cut 4294967296 down to 0. */
static size_t _jkii_to_index(int64_t value)
{
    if (value < 0) {
        return SIZE_MAX;
    }
#if INT64_MAX > SIZE_MAX
    if (value > (int64_t)SIZE_MAX) {
        return SIZE_MAX;
    }
#endif
    return (size_t)value;
}

/* Reads the segment at the start of path, "/field" or "/[index]", into
 * target. Returns 0 if it is one, with *next at the '/' that starts the next
 * segment, or NULL if it is the last. Returns -1, with *next NULL, if it is
 * neither, such as an empty field, an index that does not read as a number,
 * or a '[' with no ']' at the end of the segment. The path then finds
 * nothing: taken for the last segment, with target left empty, it would find
 * the value of a "" key. */
static int _jkii_get_target(
        const char* path,
        _jkii_target_t* target,
        const char** next)
{
    const char* start = NULL;
    const char* retval = NULL;
    size_t path_len = strlen(path);
    size_t target_len = 0;
    int before_is_bash_slash = 0;
    int ret = -1;

    M_JKII_ASSERT(path != NULL);
    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(next != NULL);

    if (path_len <= 1 || *path != '/' || strncmp(path, "//", 2) == 0) {
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
        int64_t value = 0;
        ++start;
        /* The ']' has to end the segment, or "[10" would be read as 1. */
        if (target_len < 2 || start[target_len - 2] != ']') {
            retval = NULL;
            goto exit;
        }
        target_len -= 2;
        if (_jkii_is_integer(start, target_len) == 0) {
            retval = NULL;
            goto exit;
        } else if (_jkii_to_int64(start, target_len, &value)
                != JKII_PRIMITIVE_ERR_OK) {
            retval = NULL;
            goto exit;
        }
        target->field.index = _jkii_to_index(value);
        target->parent_type = _JKII_PARENT_TYPE_ARRAY;
    } else {
        target->field.name = start;
        target->len = target_len;
        target->parent_type = _JKII_PARENT_TYPE_OBJECT;
    }
    ret = 0;

exit:
    *next = retval;
    return ret;
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
        size_t tokens_num,
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
        if (_jkii_get_target(next_root, &target, &next_root) != 0) {
            return -1;
        }
        if (root_token->type == JSMN_OBJECT &&
                target.parent_type == _JKII_PARENT_TYPE_OBJECT) {
            size_t i = 0;
            size_t index = 1;
            const size_t root_index = (size_t)(root_token - tokens);
            const size_t root_token_size = (size_t)root_token->size;
            const jsmntok_t* next_token = NULL;
            /* As in _kii_jsmn_get_value(), a key may be the last token. */
            for (i = 0; i < root_token_size &&
                    root_index + index + 1 < tokens_num; ++i) {
                const jsmntok_t* key_token = root_token + index;
                const jsmntok_t* value_token = root_token + index + 1;
                /* jsmn is not strict: an unquoted key is a primitive. */
                if (key_token->type != JSMN_STRING) {
                    return -1;
                }
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
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    /* get actual type, and an integer's value with it. */
    type = _jkii_to_jkii_field_type(value->type,
            json_string + value->start, value->end - value->start,
            &integer, &integer_res);
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
            /* An integer too large for an int32_t is still an integer: the
             * conversion reports it as out of range. */
            if (type != JKII_FIELD_TYPE_INTEGER &&
                    type != JKII_FIELD_TYPE_LONG) {
                field->type = type;
                field->result = JKII_FIELD_ERR_TYPE_MISMATCH;
                retval = JKII_ERR_PARTIAL;
            } else {
                jkii_primitive_err_t result = _jkii_narrow_to_int32(
                        integer, integer_res, &(field->field_copy.int_value));
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
                jkii_primitive_err_t result = integer_res;
                if (result != JKII_PRIMITIVE_ERR_INVALID) {
                    field->field_copy.long_value = integer;
                }
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
    jkii_resource_t* resource,
    size_t tokens_num)
{
    jkii_parse_err_t ret = JKII_ERR_OK;
    for (jkii_field_t* field = fields; field->name != NULL || field->path != NULL; ++field) {
        int result = -1;
        jsmntok_t* value = NULL;

        /* get jsmntok_t pointing target value. */
        if (field->path != NULL) {
            result = _kii_jsmn_get_value_by_path(json_string,
                    json_string_len, resource->tokens, tokens_num,
                    field->path, &value);
        } else {
            result = _kii_jsmn_get_value(json_string, json_string_len,
                    resource->tokens, tokens_num, field->name, &value);
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

    if (resource == NULL || resource->tokens_num == 0) {
        return JKII_ERR_TOKENS_SHORTAGE;
    }

    jkii_parse_err_t res = JKII_ERR_INVALID_INPUT;
    size_t tokens_num = 0;
    res = _kii_jsmn_get_tokens(json_string, json_string_len, resource,
            &tokens_num);
    if (res != JKII_ERR_OK) {
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        return JKII_ERR_INVALID_INPUT;
    }
    res = _jkii_parse_fields(json_string, json_string_len, fields, resource,
            tokens_num);

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

    size_t tokens_num = 0;
    jkii_parse_err_t res = _kii_jsmn_get_tokens(json_string, json_string_len, resource, &tokens_num);
    if (res != JKII_ERR_OK) {
        cb_free(resource);
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_ARRAY && type != JSMN_OBJECT) {
        cb_free(resource);
        return JKII_ERR_INVALID_INPUT;
    }
    res = _jkii_parse_fields(json_string, json_string_len, fields, resource,
            tokens_num);

    cb_free(resource);
    return res;
}

jkii_primitive_err_t jkii_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    jkii_primitive_t* result)
{
    if (M_JKII_EQ_LITERAL(primitive, primitive_length, "null"))
    {
        result->type = JKII_FIELD_TYPE_NULL;
        return JKII_PRIMITIVE_ERR_OK;
    }
    if (M_JKII_EQ_LITERAL(primitive, primitive_length, "true"))
    {
        result->type = JKII_FIELD_TYPE_BOOLEAN;
        result->value.boolean_value = JKII_TRUE;
        return JKII_PRIMITIVE_ERR_OK;
    }
    if (M_JKII_EQ_LITERAL(primitive, primitive_length, "false"))
    {
        result->type = JKII_FIELD_TYPE_BOOLEAN;
        result->value.boolean_value = JKII_FALSE;
        return JKII_PRIMITIVE_ERR_OK;
    }
    int is_integer = _jkii_is_integer(primitive, primitive_length);
    if (is_integer != 0) {
        jkii_primitive_t integer;
        jkii_primitive_err_t res =
            _jkii_to_integer(primitive, primitive_length, &integer);
        if (res != JKII_PRIMITIVE_ERR_OK) {
            return res;
        }
        *result = integer;
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

static int _jkii_escape_fail(char* buff, size_t buff_size) {
    // Callers that ignore the result still print buff, so leave it a string.
    if (buff_size > 0) {
        buff[0] = '\0';
    }
    return -1;
}

int jkii_escape_str(const char* str, char* buff, size_t buff_size) {
    static const char hex_digits[] = "0123456789abcdef";
    size_t str_len = strlen(str);
    if (buff_size < str_len + 1) {
        return _jkii_escape_fail(buff, buff_size);
    }
    size_t pos = 0;
    for (size_t i = 0; i < str_len; ++i) {
        // Unsigned, so that UTF-8 bytes do not compare below 0x20.
        unsigned char c = (unsigned char)str[i];
        char short_esc = '\0';
        switch (c) {
            case '"':
            case '/':
            case '\\':
                short_esc = (char)c;
                break;
            case '\b':
                short_esc = 'b';
                break;
            case '\f':
                short_esc = 'f';
                break;
            case '\n':
                short_esc = 'n';
                break;
            case '\r':
                short_esc = 'r';
                break;
            case '\t':
                short_esc = 't';
                break;
        }
        // Other control characters have no short form and take \u00XX.
        size_t esc_len = 1;
        if (short_esc != '\0') {
            esc_len = 2;
        } else if (c < 0x20) {
            esc_len = 6;
        }
        // pos < buff_size throughout, so this cannot wrap. The + 1 keeps
        // room for the terminator.
        if (buff_size - pos < esc_len + 1) {
            return _jkii_escape_fail(buff, buff_size);
        }
        if (short_esc != '\0') {
            buff[pos++] = '\\';
            buff[pos++] = short_esc;
        } else if (c < 0x20) {
            buff[pos++] = '\\';
            buff[pos++] = 'u';
            buff[pos++] = '0';
            buff[pos++] = '0';
            buff[pos++] = hex_digits[c >> 4];
            buff[pos++] = hex_digits[c & 0x0f];
        } else {
            buff[pos++] = (char)c;
        }
    }
    buff[pos] = '\0';
    return (int)pos;
}

jkii_parse_err_t jkii_validate_root_object(
        const char* json_string,
        size_t json_string_len,
        jkii_resource_t* resource)
{
    M_JKII_ASSERT(json_string != NULL);

    if (resource == NULL || resource->tokens_num == 0) {
        return JKII_ERR_TOKENS_SHORTAGE;
    }

    jkii_parse_err_t res = JKII_ERR_INVALID_INPUT;
    res = _kii_jsmn_get_tokens(json_string, json_string_len, resource, NULL);
    if (res != JKII_ERR_OK) {
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_OBJECT) {
        return JKII_ERR_ROOT_TYPE;
    }

    return JKII_ERR_OK;
}

jkii_parse_err_t jkii_validate_root_object_with_allocator(
        const char* json_string,
        size_t json_string_len,
        JKII_CB_RESOURCE_ALLOC cb_alloc,
        JKII_CB_RESOURCE_FREE cb_free)
{
    M_JKII_ASSERT(json_string != NULL);
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

    jkii_parse_err_t res = _kii_jsmn_get_tokens(json_string, json_string_len, resource, NULL);
    if (res != JKII_ERR_OK) {
        cb_free(resource);
        return res;
    }

    jsmntype_t type = resource->tokens[0].type;
    if (type != JSMN_OBJECT) {
        cb_free(resource);
        return JKII_ERR_ROOT_TYPE;
    }

    cb_free(resource);
    return JKII_ERR_OK;
}
