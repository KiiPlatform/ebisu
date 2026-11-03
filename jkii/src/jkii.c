#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <ctype.h>
#include <locale.h>

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
#include "jkii_core_json.h"

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

/* A value in the document: where it is, its length and its type, as coreJSON
 * gives them. A string's is its characters between its quotes. */
typedef struct _jkii_value_t {
    const char* ptr;
    size_t len;
    JSONTypes_t type;
} _jkii_value_t;

static int _jkii_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/* Checks the whole document, which has to be JSON, and gives its root. */
static jkii_parse_err_t _jkii_root(
        const char* json_string,
        size_t json_string_len,
        _jkii_value_t* out_root)
{
    size_t i = 0;
    if (json_string_len == 0 ||
            JSON_Validate(json_string, json_string_len) != JSONSuccess) {
        /* Empty or whitespace only too, which can come from the network. */
        return JKII_ERR_INVALID_INPUT;
    }
    while (i < json_string_len && _jkii_is_space(json_string[i])) {
        ++i;
    }
    out_root->ptr = json_string;
    out_root->len = json_string_len;
    switch (json_string[i]) {
        case '{':
            out_root->type = JSONObject;
            break;
        case '[':
            out_root->type = JSONArray;
            break;
        default:
            /* A string, a number, true, false or null. */
            out_root->type = JSONInvalid;
            break;
    }
    return JKII_ERR_OK;
}

static void _jkii_value_of_pair(const JSONPair_t* pair, _jkii_value_t* out)
{
    out->ptr = pair->value;
    out->len = pair->valueLength;
    out->type = pair->jsonType;
}

/* The value of the first member of object whose key is name, compared as
 * written, escapes and all. */
static int _jkii_find_member_by_name(
        const _jkii_value_t* object,
        const char* name,
        _jkii_value_t* out_value)
{
    size_t start = 0;
    size_t next = 0;
    size_t name_len = strlen(name);
    JSONPair_t pair;
    while (JSON_Iterate(object->ptr, object->len, &start, &next, &pair) ==
            JSONSuccess) {
        if (pair.keyLength == name_len &&
                memcmp(pair.key, name, name_len) == 0) {
            _jkii_value_of_pair(&pair, out_value);
            return 0;
        }
    }
    return -1;
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
        JSONTypes_t json_type,
        const char* buf,
        size_t buf_len,
        int64_t* out_integer,
        jkii_primitive_err_t* out_integer_res)
{
    switch (json_type)
    {
        case JSONNumber:
            if (_jkii_is_integer(buf, buf_len) != 0) {
                *out_integer_res = _jkii_to_int64(buf, buf_len, out_integer);
                return _jkii_integer_type(*out_integer, *out_integer_res);
            } else if (_jkii_is_double(buf, buf_len) != 0) {
                return JKII_FIELD_TYPE_DOUBLE;
            }
            /* coreJSON has checked it is a number. */
            return JKII_FIELD_TYPE_ANY;
        case JSONTrue:
        case JSONFalse:
            return JKII_FIELD_TYPE_BOOLEAN;
        case JSONNull:
            return JKII_FIELD_TYPE_NULL;
        case JSONObject:
            return JKII_FIELD_TYPE_OBJECT;
        case JSONArray:
            return JKII_FIELD_TYPE_ARRAY;
        case JSONString:
            return JKII_FIELD_TYPE_STRING;
        case JSONInvalid:
        default:
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

/* Puts the locale's decimal point in place of the '.' in the number of len
 * bytes in buf, terminated, which holds size. strtod() reads the decimal point
 * the locale sets, a comma in many, while JSON's is always '.', so a program
 * that set such a locale had no number with a fraction read. Returns 0, or -1
 * if the number no longer fits, with a decimal point of more than a byte. */
static int _jkii_localize_decimal_point(char* buf, size_t size, size_t len)
{
    const char* point = localeconv()->decimal_point;
    size_t point_len = strlen(point);
    char* dot = (char*)memchr(buf, '.', len);
    if (dot == NULL || point_len == 0 || (point_len == 1 && point[0] == '.')) {
        return 0;
    }
    if (len - 1 + point_len >= size) {
        return -1;
    }
    /* What follows the '.', and the terminator. */
    memmove(dot + point_len, dot + 1, len - (size_t)(dot - buf));
    memcpy(dot, point, point_len);
    return 0;
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
    if (_jkii_localize_decimal_point(buf, buf_len, target_size) != 0) {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

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
 * the size_t range is SIZE_MAX, which no array reaches, as it could not fit in
 * memory. A cast alone makes -1 SIZE_MAX as well, but where size_t has 32 bits
 * it would cut 4294967296 down to 0. */
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

/* The value the path leads to from root, a segment at a time: a member of an
 * object, or an element of an array. */
static int _jkii_find_by_path(
        const _jkii_value_t* root,
        const char* path,
        _jkii_value_t* out_value)
{
    const char* next_root = path;
    _jkii_value_t current = *root;

    M_JKII_ASSERT(path != NULL && strlen(path) > 0);
    M_JKII_ASSERT(out_value != NULL);

    do {
        _jkii_target_t target;
        size_t start = 0;
        size_t next = 0;
        size_t i = 0;
        int found = 0;
        JSONPair_t pair;
        memset(&target, 0x00, sizeof(target));
        if (_jkii_get_target(next_root, &target, &next_root) != 0) {
            return -1;
        }
        if (current.type == JSONObject &&
                target.parent_type == _JKII_PARENT_TYPE_OBJECT) {
            while (!found && JSON_Iterate(current.ptr, current.len, &start,
                            &next, &pair) == JSONSuccess) {
                found = _jkii_is_same_key(&target, pair.key,
                        pair.keyLength) == 0;
            }
        } else if (current.type == JSONArray &&
                target.parent_type == _JKII_PARENT_TYPE_ARRAY) {
            while (!found && JSON_Iterate(current.ptr, current.len, &start,
                            &next, &pair) == JSONSuccess) {
                found = (i++ == target.field.index);
            }
        }
        if (!found) {
            return -1;
        }
        _jkii_value_of_pair(&pair, &current);
    } while (next_root != NULL);

    *out_value = current;
    return 0;
}

static jkii_parse_err_t _jkii_convert_value_to_field(
        jkii_field_t* field,
        const _jkii_value_t* value,
        const char* json_string)
{
    jkii_parse_err_t retval = JKII_ERR_OK;
    jkii_field_type_t type = JKII_FIELD_TYPE_ANY;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    /* get actual type, and an integer's value with it. */
    type = _jkii_to_jkii_field_type(value->type, value->ptr, value->len,
            &integer, &integer_res);
    if (type == JKII_FIELD_TYPE_ANY) {
        /* fail to get actual type. */
        return JKII_ERR_INVALID_INPUT;
    }

    if (field->type == JKII_FIELD_TYPE_ANY) {
        field->type = type;
    }

    field->start = (size_t)(value->ptr - json_string);
    field->end = field->start + value->len;

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
                            value->ptr,
                            value->len,
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
                            value->ptr,
                            value->len,
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
            } else if (_jkii_to_boolean(value->ptr, value->len,
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
            } else if (M_JKII_EQ_LITERAL(value->ptr, value->len, "null")) {
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
    const _jkii_value_t* root,
    jkii_field_t* fields)
{
    jkii_parse_err_t ret = JKII_ERR_OK;
    for (jkii_field_t* field = fields; field->name != NULL || field->path != NULL; ++field) {
        int result = -1;
        _jkii_value_t value;

        if (field->path != NULL) {
            result = _jkii_find_by_path(root, field->path, &value);
        } else if (root->type == JSONObject) {
            result = _jkii_find_member_by_name(root, field->name, &value);
        }
        if (result != 0)
        {
            ret = JKII_ERR_PARTIAL;
            field->result = JKII_FIELD_ERR_NOT_FOUND;
            continue;
        }

        switch (_jkii_convert_value_to_field(field, &value, json_string)) {
            case JKII_ERR_OK:
                break;
            case JKII_ERR_PARTIAL:
                ret = JKII_ERR_PARTIAL;
                break;
            case JKII_ERR_INVALID_INPUT:
                return JKII_ERR_INVALID_INPUT;
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
    jkii_field_t* fields)
{
    _jkii_value_t root;
    M_JKII_ASSERT(json_string != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return res;
    }
    if (root.type != JSONArray && root.type != JSONObject) {
        return JKII_ERR_INVALID_INPUT;
    }
    return _jkii_parse_fields(json_string, &root, fields);
}

jkii_parse_err_t jkii_next_member(
        const char* json_string,
        size_t json_string_len,
        jkii_member_iter_t* iter,
        jkii_member_t* out_member)
{
    JSONPair_t pair;
    JSONStatus_t status = JSONNotFound;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(iter != NULL);
    M_JKII_ASSERT(out_member != NULL);

    /* Checked whole on the first call, as JSON_Iterate() requires. */
    if (iter->_start == 0 && iter->_next == 0) {
        _jkii_value_t root;
        jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
        if (res != JKII_ERR_OK) {
            return res;
        }
        if (root.type != JSONArray && root.type != JSONObject) {
            return JKII_ERR_ROOT_TYPE;
        }
    }
    status = JSON_Iterate(json_string, json_string_len, &iter->_start,
            &iter->_next, &pair);
    if (status == JSONNotFound) {
        return JKII_ERR_NOT_FOUND;
    } else if (status != JSONSuccess) {
        return JKII_ERR_INVALID_INPUT;
    }
    out_member->key = pair.key;
    out_member->key_length = pair.keyLength;
    out_member->value = pair.value;
    out_member->value_length = pair.valueLength;
    out_member->type = _jkii_to_jkii_field_type(pair.jsonType, pair.value,
            pair.valueLength, &integer, &integer_res);
    return JKII_ERR_OK;
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

/* The UTF-16 code unit of the \u escape at s, the backslash, of which len
 * bytes are left, or -1 if it isn't one. */
static long _jkii_unicode_escape(const char* s, size_t len)
{
    long unit = 0;
    if (len < 6 || s[0] != '\\' || s[1] != 'u') {
        return -1;
    }
    for (size_t i = 2; i < 6; ++i) {
        char c = s[i];
        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return -1;
        }
        unit = unit * 16 + digit;
    }
    return unit;
}

/* Writes a code point, which is not a surrogate, as UTF-8, and returns how
 * many bytes that took. */
static size_t _jkii_utf8(unsigned long cp, char* out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        return 3;
    }
    out[0] = (char)(0xf0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    return 4;
}

int jkii_unescape_str(const char* str, size_t str_len, char* buff, size_t buff_size) {
    size_t pos = 0;
    size_t i = 0;
    M_JKII_ASSERT(str != NULL || str_len == 0);
    if (buff_size == 0) {
        return -1;
    }
    while (i < str_len) {
        unsigned char c = (unsigned char)str[i];
        char out[4];
        size_t out_len = 1;
        if (c == '"' || c < 0x20) {
            return _jkii_escape_fail(buff, buff_size);
        } else if (c != '\\') {
            out[0] = (char)c;
            i += 1;
        } else if (str_len - i < 2) {
            return _jkii_escape_fail(buff, buff_size);
        } else if (str[i + 1] != 'u') {
            switch (str[i + 1]) {
                case '"':
                case '\\':
                case '/':
                    out[0] = str[i + 1];
                    break;
                case 'b':
                    out[0] = '\b';
                    break;
                case 'f':
                    out[0] = '\f';
                    break;
                case 'n':
                    out[0] = '\n';
                    break;
                case 'r':
                    out[0] = '\r';
                    break;
                case 't':
                    out[0] = '\t';
                    break;
                default:
                    return _jkii_escape_fail(buff, buff_size);
            }
            i += 2;
        } else {
            long unit = _jkii_unicode_escape(&str[i], str_len - i);
            unsigned long cp = (unsigned long)unit;
            // -1 isn't an escape, and 0 would end the string. A low
            // surrogate can only come second.
            if (unit <= 0 || (unit >= 0xdc00 && unit <= 0xdfff)) {
                return _jkii_escape_fail(buff, buff_size);
            }
            i += 6;
            if (unit >= 0xd800 && unit <= 0xdbff) {
                long low = _jkii_unicode_escape(&str[i], str_len - i);
                if (low < 0xdc00 || low > 0xdfff) {
                    return _jkii_escape_fail(buff, buff_size);
                }
                i += 6;
                cp = 0x10000 + (((unsigned long)unit - 0xd800) << 10) +
                    ((unsigned long)low - 0xdc00);
            }
            out_len = _jkii_utf8(cp, out);
        }
        // pos < buff_size throughout, so this cannot wrap. The + 1 keeps
        // room for the terminator.
        if (buff_size - pos < out_len + 1 || pos + out_len > (size_t)INT_MAX) {
            return _jkii_escape_fail(buff, buff_size);
        }
        memcpy(&buff[pos], out, out_len);
        pos += out_len;
    }
    buff[pos] = '\0';
    return (int)pos;
}

jkii_parse_err_t jkii_validate_root_object(
        const char* json_string,
        size_t json_string_len)
{
    _jkii_value_t root;
    M_JKII_ASSERT(json_string != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return res;
    }
    if (root.type != JSONObject) {
        return JKII_ERR_ROOT_TYPE;
    }
    return JKII_ERR_OK;
}
