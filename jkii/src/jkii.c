#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <float.h>
#include <stdbool.h>
#ifndef EBISU_NO_STRTOD
#include <locale.h>
#endif

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
#include <ebisu_str.h>
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

/* "+ 3" denotes '-', '.', '\0': the longest number read as a double has
 * DBL_MAX_10_EXP + 2 characters, 310. */
#define DOUBLEBUFSIZE (DBL_MAX_10_EXP + 3)

/* A decimal digit, as JSON's are: not isdigit(), which a C library may look up
 * in the locale's tables. */
#define M_JKII_IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

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
        case '"':
            out_root->type = JSONString;
            break;
        case 't':
            out_root->type = JSONTrue;
            break;
        case 'f':
            out_root->type = JSONFalse;
            break;
        case 'n':
            out_root->type = JSONNull;
            break;
        default:
            /* A number, the only value left that JSON_Validate() takes. */
            out_root->type = JSONNumber;
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
        if (!M_JKII_IS_DIGIT(buf[i])) {
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
    /* With its terminator, it has to fit the buffer _jkii_to_double() copies
     * it into for strtod(). */
    if (buf_len >= DOUBLEBUFSIZE) {
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
                if (M_JKII_IS_DIGIT(c)) {
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
                if (M_JKII_IS_DIGIT(c)) {
                    continue;
                }
                if (c == 'e' || c == 'E') {
                    sts = parser_sts_exp_start;
                    continue;
                }
                return 0;
            case parser_sts_exp_start:
                if (c == '-' || c == '+' || M_JKII_IS_DIGIT(c)) {
                    sts = parser_sts_exp;
                    continue;
                }
                return 0;
            case parser_sts_exp:
                if (M_JKII_IS_DIGIT(c)) {
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
    const char* digits = target;
    size_t digits_len = target_size;
    int64_t value = 0;

    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(out_int64 != NULL);

    /* A JSON number has no leading zeros: 0 and -0 are numbers, 00, 007 and
     * 010 are not, though core's parser takes them. Nor has it a '+', which it
     * takes too, but _jkii_is_integer() has let through only a '-' and
     * digits. */
    if (digits_len > 0 && *digits == '-') {
        ++digits;
        --digits_len;
    }
    if (digits_len > 1 && digits[0] == '0') {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

    /* Decimal, in an int64_t, without strtoll(), errno or a copy: digits of
     * any length past the range are a range error, whose sign says which. */
    switch (ebisu_parse_int(target, target_size, INT64_MIN, INT64_MAX, &value)) {
    case EBISU_PARSE_OK:
        *out_int64 = value;
        return JKII_PRIMITIVE_ERR_OK;
    case EBISU_PARSE_RANGE:
        if (*target == '-') {
            *out_int64 = INT64_MIN;
            return JKII_PRIMITIVE_ERR_UNDERFLOW;
        }
        *out_int64 = INT64_MAX;
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    default:
        return JKII_PRIMITIVE_ERR_INVALID;
    }
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

#ifndef EBISU_NO_STRTOD
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
        } else if (value > 1 || value < -1) {
            /* Plus or minus HUGE_VAL. */
            *out_double = value;
            return JKII_PRIMITIVE_ERR_OVERFLOW;
        }
        /* Below the smallest normal double, but not 0: strtod() may say
         * ERANGE for it, as glibc's and the UCRT's do, though it is a double
         * all the same. */
    }

    *out_double = value;
    return JKII_PRIMITIVE_ERR_OK;
}
#else
/* Powers of ten a double holds exactly: 10^22 is the largest, as 5^22 is below
 * 2^53. */
static const double _jkii_exact_pow10[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
};
/* 10 to the powers of two, for an exponent beyond those: a double's range is
 * within 10^-512 to 10^512 of any 19 digits. */
static const double _jkii_pow10_by_bit[] = {
    1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256
};

/* The digits of a decimal number at s, up to len or what isn't one: as many as
 * fit in an unsigned 64-bit integer, 19, with the power of ten that each
 * further one before the point adds. Returns where they end. */
static size_t _jkii_read_digits(
        const char* s,
        size_t len,
        size_t i,
        int fraction,
        uint64_t* digits,
        long* exp10)
{
    for (; i < len && M_JKII_IS_DIGIT(s[i]); ++i) {
        if (*digits < UINT64_C(1000000000000000000)) {
            *digits = *digits * 10 + (uint64_t)(s[i] - '0');
            if (fraction) {
                --*exp10;
            }
        } else if (!fraction) {
            ++*exp10;
        }
    }
    return i;
}

/* A JSON number as a double, without strtod(): its digits as an integer, and
 * the power of ten they are scaled by. With 2^53 or fewer and a power within
 * 10^-22 to 10^22, one multiplication or division of two exact doubles, which
 * IEEE 754 rounds once, gives the double nearest the number, as strtod() does.
 * Beyond, the scaling takes a few steps, each rounded, and the result can be a
 * few units in the last place from it. So can a 32-bit x86 build's that
 * computes on the x87, which rounds twice. Nothing of the locale's plays a
 * part. */
static jkii_primitive_err_t _jkii_to_double(
        const char* s,
        size_t len,
        double* out_double)
{
    uint64_t digits = 0;
    long exp10 = 0;
    int negative = 0;
    size_t i = 0;
    size_t start = 0;
    double value = 0;

    M_JKII_ASSERT(out_double != NULL);

    if (i < len && s[i] == '-') {
        negative = 1;
        ++i;
    }
    start = i;
    i = _jkii_read_digits(s, len, i, 0, &digits, &exp10);
    if (i == start) {
        return JKII_PRIMITIVE_ERR_INVALID;
    }
    /* A point needn't have digits after it, as strtod() takes "1." too. */
    if (i < len && s[i] == '.') {
        i = _jkii_read_digits(s, len, i + 1, 1, &digits, &exp10);
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        int exp_negative = 0;
        long e = 0;
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            exp_negative = s[i] == '-';
            ++i;
        }
        start = i;
        for (; i < len && M_JKII_IS_DIGIT(s[i]); ++i) {
            /* Beyond that, any digits make 0 or an infinity already. */
            if (e < 100000) {
                e = e * 10 + (s[i] - '0');
            }
        }
        if (i == start) {
            return JKII_PRIMITIVE_ERR_INVALID;
        }
        exp10 += exp_negative ? -e : e;
    }
    if (i != len) {
        return JKII_PRIMITIVE_ERR_INVALID;
    }

    if (digits == 0) {
        /* -0 is a number of its own, as strtod() reads it. */
        *out_double = negative ? -0.0 : 0.0;
        return JKII_PRIMITIVE_ERR_OK;
    }
    /* Zeros at the end of the digits, "1.50" or "1500", are a power of ten. And
     * a power above 10^22 goes into the digits while they stay at 2^53 or below,
     * so that 1e23 is 10 times 10^22. Either keeps the one exact operation. */
    while (digits % 10 == 0) {
        digits /= 10;
        ++exp10;
    }
    while (exp10 > 22 && digits <= ((UINT64_C(1) << 53) / 10)) {
        digits *= 10;
        --exp10;
    }
    value = (double)digits;
    if (digits <= (UINT64_C(1) << 53) && exp10 >= -22 && exp10 <= 22) {
        value = exp10 < 0 ? value / _jkii_exact_pow10[-exp10] : value * _jkii_exact_pow10[exp10];
    } else {
        unsigned long n = (unsigned long)(exp10 < 0 ? -exp10 : exp10);
        size_t k;
        if (n >= 512) {
            /* Beyond a double whatever the digits: 0, or an infinity. */
            const double big = _jkii_pow10_by_bit[8];
            value = exp10 < 0 ? value / big / big : value * big * big;
            n = 0;
        }
        /* The smaller powers first, so that the value moves one way only and
         * reaches 0 or an infinity only if the number is beyond a double. */
        for (k = 0; n != 0; ++k, n >>= 1) {
            if (n & 1) {
                value = exp10 < 0 ? value / _jkii_pow10_by_bit[k] : value * _jkii_pow10_by_bit[k];
            }
        }
    }
    if (negative) {
        value = -value;
    }
    *out_double = value;
    if (value == 0) {
        return JKII_PRIMITIVE_ERR_UNDERFLOW;
    } else if (value - value != value - value) {
        /* An infinity, which less itself is NaN. */
        return JKII_PRIMITIVE_ERR_OVERFLOW;
    }
    return JKII_PRIMITIVE_ERR_OK;
}
#endif

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
    int ret = -1;

    M_JKII_ASSERT(path != NULL);
    M_JKII_ASSERT(target != NULL);
    M_JKII_ASSERT(next != NULL);

    if (path_len <= 1 || *path != '/' || strncmp(path, "//", 2) == 0) {
        retval = NULL;
        goto exit;
    }

    /* get length of target: up to the next '/', a backslash escaping the
       character after it, '/' or another backslash among them, which are
       taken together. */
    start = path + 1;
    for (target_len = 0, retval = start; *retval != '\0' && *retval != '/';
            ++retval, ++target_len) {
        if (*retval == '\\' && retval[1] != '\0') {
            ++retval;
            ++target_len;
        }
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

/* Each field not found, in a string nothing can be looked up in, so that no
   field reads as found from what its result was before the call. */
static jkii_parse_err_t _jkii_none_found(jkii_field_t* fields, jkii_parse_err_t res)
{
    for (jkii_field_t* field = fields; field->name != NULL || field->path != NULL; ++field) {
        field->result = JKII_FIELD_ERR_NOT_FOUND;
    }
    return res;
}

jkii_parse_err_t jkii_parse(
    const char* json_string,
    size_t json_string_len,
    jkii_field_t* fields)
{
    _jkii_value_t root;
    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(fields != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return _jkii_none_found(fields, res);
    }
    if (root.type != JSONArray && root.type != JSONObject) {
        return _jkii_none_found(fields, JKII_ERR_INVALID_INPUT);
    }
    res = _jkii_parse_fields(json_string, &root, fields);
    if (res == JKII_ERR_INVALID_INPUT) {
        /* A field's value is JSON that jkii doesn't read, a number too long:
         * the fields before it found, and those after it as they were. */
        return _jkii_none_found(fields, res);
    }
    return res;
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

jkii_parse_err_t jkii_root_value(
        const char* json_string,
        size_t json_string_len,
        jkii_member_t* out_value)
{
    _jkii_value_t root;
    size_t start = 0;
    size_t end = json_string_len;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(out_value != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return res;
    }
    /* JSON_Validate() has taken a value, so there is one between the spaces. */
    while (_jkii_is_space(json_string[start])) {
        ++start;
    }
    while (_jkii_is_space(json_string[end - 1])) {
        --end;
    }
    if (root.type == JSONString) {
        /* Its characters between its quotes, as for a member. */
        ++start;
        --end;
    }
    out_value->key = NULL;
    out_value->key_length = 0;
    out_value->value = &json_string[start];
    out_value->value_length = end - start;
    out_value->type = _jkii_to_jkii_field_type(root.type, out_value->value,
            out_value->value_length, &integer, &integer_res);
    return JKII_ERR_OK;
}

jkii_parse_err_t jkii_find_member(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        jkii_member_t* out_member)
{
    _jkii_value_t root;
    _jkii_value_t value;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(path != NULL);
    M_JKII_ASSERT(out_member != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return res;
    }
    if (root.type != JSONArray && root.type != JSONObject) {
        return JKII_ERR_ROOT_TYPE;
    }
    if (path[0] == '\0' || _jkii_find_by_path(&root, path, &value) != 0) {
        return JKII_ERR_NOT_FOUND;
    }
    out_member->key = NULL;
    out_member->key_length = 0;
    out_member->value = value.ptr;
    out_member->value_length = value.len;
    out_member->type = _jkii_to_jkii_field_type(value.type, value.ptr, value.len,
            &integer, &integer_res);
    return JKII_ERR_OK;
}

jkii_field_err_t jkii_member_string(
        const jkii_member_t* member,
        char* buff,
        size_t buff_size)
{
    M_JKII_ASSERT(member != NULL);
    if (buff_size > 0) {
        buff[0] = '\0';
    }
    if (member->type == JKII_FIELD_TYPE_NULL) {
        return JKII_FIELD_ERR_NULL;
    }
    if (member->type != JKII_FIELD_TYPE_STRING) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    if (jkii_unescape_str(member->value, member->value_length, buff, buff_size) < 0) {
        return JKII_FIELD_ERR_COPY;
    }
    return JKII_FIELD_ERR_OK;
}

/* A JSON number's value as an integer, read from its digits: the integer
 * part's, then the fraction's, with the decimal point moved by the exponent.
 * Those after the point have to be zeros, or the number isn't whole. The
 * number is valid JSON, as coreJSON has checked it. */
static jkii_field_err_t _jkii_whole_number(const char* s, size_t len, int64_t* out_value)
{
    size_t i = 0;
    bool negative = false;
    size_t int_start = 0;
    size_t int_len = 0;
    size_t frac_start = 0;
    size_t frac_len = 0;
    int64_t exponent = 0;
    bool exponent_negative = false;
    /* How many of the digits come before the point, as the exponent puts it:
       less than 0, or more than there are digits, as it may. */
    int64_t point = 0;
    size_t digits = 0;
    size_t k = 0;
    uint64_t magnitude = 0;
    uint64_t limit = 0;

    if (i < len && s[i] == '-') {
        negative = true;
        ++i;
    }
    int_start = i;
    while (i < len && s[i] >= '0' && s[i] <= '9') {
        ++i;
    }
    int_len = i - int_start;
    if (i < len && s[i] == '.') {
        frac_start = ++i;
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            ++i;
        }
        frac_len = i - frac_start;
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            exponent_negative = (s[i] == '-');
            ++i;
        }
        while (i < len && s[i] >= '0' && s[i] <= '9') {
            /* Held there, far past int64_t's digits either way: a number that
               big overflows, and one that small isn't whole, unless 0. */
            if (exponent < 100000) {
                exponent = exponent * 10 + (s[i] - '0');
            }
            ++i;
        }
    }
    digits = int_len + frac_len;
    point = (int64_t)int_len + (exponent_negative ? -exponent : exponent);
    limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
    for (k = 0; k < digits; ++k) {
        unsigned int digit = (unsigned int)((k < int_len ?
                    s[int_start + k] : s[frac_start + (k - int_len)]) - '0');
        if ((int64_t)k >= point) {
            if (digit != 0) {
                return JKII_FIELD_ERR_TYPE_MISMATCH;
            }
            continue;
        }
        if (magnitude > (limit - digit) / 10u) {
            return negative ? JKII_FIELD_ERR_NUM_UNDERFLOW : JKII_FIELD_ERR_NUM_OVERFLOW;
        }
        magnitude = magnitude * 10u + digit;
    }
    /* The zeros the exponent puts after the digits. */
    for (k = digits; (int64_t)k < point && magnitude != 0; ++k) {
        if (magnitude > limit / 10u) {
            return negative ? JKII_FIELD_ERR_NUM_UNDERFLOW : JKII_FIELD_ERR_NUM_OVERFLOW;
        }
        magnitude *= 10u;
    }
    if (negative) {
        *out_value = magnitude == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return JKII_FIELD_ERR_OK;
}

jkii_field_err_t jkii_member_integer(
        const jkii_member_t* member,
        int64_t min,
        int64_t max,
        int64_t* out_value)
{
    int64_t value = 0;
    jkii_field_err_t res;
    M_JKII_ASSERT(member != NULL);
    M_JKII_ASSERT(out_value != NULL);
    if (member->type == JKII_FIELD_TYPE_NULL) {
        return JKII_FIELD_ERR_NULL;
    }
    if (member->type != JKII_FIELD_TYPE_INTEGER && member->type != JKII_FIELD_TYPE_LONG &&
            member->type != JKII_FIELD_TYPE_DOUBLE) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    res = _jkii_whole_number(member->value, member->value_length, &value);
    if (res != JKII_FIELD_ERR_OK) {
        return res;
    }
    if (value < min) {
        return JKII_FIELD_ERR_NUM_UNDERFLOW;
    }
    if (value > max) {
        return JKII_FIELD_ERR_NUM_OVERFLOW;
    }
    *out_value = value;
    return JKII_FIELD_ERR_OK;
}

jkii_field_err_t jkii_member_double(
        const jkii_member_t* member,
        double* out_value)
{
    double value = 0;
    M_JKII_ASSERT(member != NULL);
    M_JKII_ASSERT(out_value != NULL);
    if (member->type == JKII_FIELD_TYPE_NULL) {
        return JKII_FIELD_ERR_NULL;
    }
    if (member->type != JKII_FIELD_TYPE_INTEGER && member->type != JKII_FIELD_TYPE_LONG &&
            member->type != JKII_FIELD_TYPE_DOUBLE) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    switch (_jkii_to_double(member->value, member->value_length, &value)) {
        case JKII_PRIMITIVE_ERR_OK:
            *out_value = value;
            return JKII_FIELD_ERR_OK;
        case JKII_PRIMITIVE_ERR_OVERFLOW:
            return JKII_FIELD_ERR_NUM_OVERFLOW;
        case JKII_PRIMITIVE_ERR_UNDERFLOW:
            return JKII_FIELD_ERR_NUM_UNDERFLOW;
        default:
            /* Too long for the buffer strtod() is given, or not a number at
             * all, in a member not from jkii. */
            return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
}

jkii_field_err_t jkii_member_boolean(
        const jkii_member_t* member,
        jkii_boolean_t* out_value)
{
    M_JKII_ASSERT(member != NULL);
    M_JKII_ASSERT(out_value != NULL);
    if (member->type == JKII_FIELD_TYPE_NULL) {
        return JKII_FIELD_ERR_NULL;
    }
    if (member->type != JKII_FIELD_TYPE_BOOLEAN ||
            _jkii_to_boolean(member->value, member->value_length, out_value) != 0) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    return JKII_FIELD_ERR_OK;
}

/* Whether s is exactly a JSON number, as ECMA-404 has it:
 * -?(0|[1-9][0-9]*)(.[0-9]+)?([eE][+-]?[0-9]+)? */
static bool _jkii_is_json_number(const char* s, size_t len)
{
    size_t i = 0;
    size_t start = 0;
    if (i < len && s[i] == '-') {
        ++i;
    }
    if (i < len && s[i] == '0') {
        ++i;
    } else if (i < len && s[i] >= '1' && s[i] <= '9') {
        while (i < len && M_JKII_IS_DIGIT(s[i])) {
            ++i;
        }
    } else {
        return false;
    }
    if (i < len && s[i] == '.') {
        start = ++i;
        while (i < len && M_JKII_IS_DIGIT(s[i])) {
            ++i;
        }
        if (i == start) {
            return false;
        }
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        start = i;
        while (i < len && M_JKII_IS_DIGIT(s[i])) {
            ++i;
        }
        if (i == start) {
            return false;
        }
    }
    return i == len;
}

const jkii_member_t* jkii_member_unquote(
        const jkii_member_t* member,
        jkii_member_t* unquoted)
{
    JSONTypes_t json_type = JSONInvalid;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(member != NULL);
    M_JKII_ASSERT(unquoted != NULL);
    if (member->type != JKII_FIELD_TYPE_STRING) {
        return member;
    }
    if (M_JKII_EQ_LITERAL(member->value, member->value_length, "true")) {
        json_type = JSONTrue;
    } else if (M_JKII_EQ_LITERAL(member->value, member->value_length, "false")) {
        json_type = JSONFalse;
    } else if (_jkii_is_json_number(member->value, member->value_length)) {
        json_type = JSONNumber;
    } else {
        return member;
    }
    *unquoted = *member;
    unquoted->type = _jkii_to_jkii_field_type(json_type, member->value,
            member->value_length, &integer, &integer_res);
    return unquoted;
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

/* Decodes the character at str[*i], of which str_len - *i bytes are left:
 * an escape, or a byte as it is, or with whole_utf8 the whole UTF-8 sequence
 * a byte starts. Writes it into out, moves *i past it, and returns its
 * length; or 0, with *i as it was, for what a string can't hold: '"', a byte
 * below 0x20, an escape that isn't one, and `\u0000` unless nul is true. */
static size_t _jkii_unescape_one(
        const char* str,
        size_t str_len,
        size_t* i,
        char out[4],
        bool nul,
        bool whole_utf8)
{
    size_t at = *i;
    unsigned char c = (unsigned char)str[at];
    if (c == '"' || c < 0x20) {
        return 0;
    }
    if (c != '\\') {
        size_t len = 1;
        if (whole_utf8) {
            len = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
            /* All there is, in a string not from jkii, which checks UTF-8. */
            len = len < str_len - at ? len : str_len - at;
        }
        memcpy(out, &str[at], len);
        *i = at + len;
        return len;
    }
    if (str_len - at < 2) {
        return 0;
    }
    if (str[at + 1] != 'u') {
        switch (str[at + 1]) {
            case '"':
            case '\\':
            case '/':
                out[0] = str[at + 1];
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
                return 0;
        }
        *i = at + 2;
        return 1;
    }
    long unit = _jkii_unicode_escape(&str[at], str_len - at);
    unsigned long cp = (unsigned long)unit;
    // -1 isn't an escape, and 0 would end a C string. A low surrogate can only
    // come second.
    if (unit < 0 || (unit == 0 && !nul) || (unit >= 0xdc00 && unit <= 0xdfff)) {
        return 0;
    }
    at += 6;
    if (unit >= 0xd800 && unit <= 0xdbff) {
        long low = _jkii_unicode_escape(&str[at], str_len - at);
        if (low < 0xdc00 || low > 0xdfff) {
            return 0;
        }
        at += 6;
        cp = 0x10000 + (((unsigned long)unit - 0xd800) << 10) +
            ((unsigned long)low - 0xdc00);
    }
    *i = at;
    return _jkii_utf8(cp, out);
}

int jkii_unescape_str(const char* str, size_t str_len, char* buff, size_t buff_size) {
    size_t pos = 0;
    size_t i = 0;
    M_JKII_ASSERT(str != NULL || str_len == 0);
    if (buff_size == 0) {
        return -1;
    }
    while (i < str_len) {
        char out[4];
        size_t out_len = _jkii_unescape_one(str, str_len, &i, out, false, false);
        if (out_len == 0) {
            return _jkii_escape_fail(buff, buff_size);
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

jkii_field_err_t jkii_member_text(
        const jkii_member_t* member,
        char* buff,
        size_t buff_size)
{
    M_JKII_ASSERT(member != NULL);
    if (buff_size > 0) {
        buff[0] = '\0';
    }
    switch (member->type) {
        case JKII_FIELD_TYPE_STRING:
            return jkii_member_string(member, buff, buff_size);
        case JKII_FIELD_TYPE_NULL:
            return JKII_FIELD_ERR_NULL;
        case JKII_FIELD_TYPE_OBJECT:
        case JKII_FIELD_TYPE_ARRAY:
            return JKII_FIELD_ERR_TYPE_MISMATCH;
        default:
            /* A number, true or false, as written: JKII_FIELD_TYPE_ANY too,
             * for a number longer than jkii reads. */
            if (member->value_length >= buff_size) {
                return JKII_FIELD_ERR_COPY;
            }
            memcpy(buff, member->value, member->value_length);
            buff[member->value_length] = '\0';
            return JKII_FIELD_ERR_OK;
    }
}

jkii_field_err_t jkii_member_string_trunc(
        const jkii_member_t* member,
        char* buff,
        size_t buff_size,
        jkii_boolean_t* out_truncated)
{
    size_t pos = 0;
    size_t i = 0;
    M_JKII_ASSERT(member != NULL);
    if (out_truncated != NULL) {
        *out_truncated = JKII_FALSE;
    }
    if (buff_size > 0) {
        buff[0] = '\0';
    }
    if (member->type == JKII_FIELD_TYPE_NULL) {
        return JKII_FIELD_ERR_NULL;
    }
    if (member->type != JKII_FIELD_TYPE_STRING) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    if (buff_size == 0) {
        return JKII_FIELD_ERR_COPY;
    }
    while (i < member->value_length) {
        char out[4];
        size_t out_len = _jkii_unescape_one(member->value, member->value_length, &i,
                out, true, true);
        if (out_len == 0) {
            /* Not a string jkii gave. */
            buff[0] = '\0';
            return JKII_FIELD_ERR_COPY;
        }
        /* The rest isn't read: what didn't fit, or a `\u0000`. */
        if (buff_size - pos < out_len + 1 || (out_len == 1 && out[0] == '\0')) {
            if (out_truncated != NULL) {
                *out_truncated = JKII_TRUE;
            }
            break;
        }
        memcpy(&buff[pos], out, out_len);
        pos += out_len;
    }
    buff[pos] = '\0';
    return JKII_FIELD_ERR_OK;
}

/* Whether key, as written, escapes and all, is name once unescaped. */
static bool _jkii_key_is(const char* key, size_t key_len, const char* name, size_t name_len)
{
    size_t i = 0;
    size_t pos = 0;
    while (i < key_len) {
        char out[4];
        size_t out_len = _jkii_unescape_one(key, key_len, &i, out, true, false);
        if (out_len == 0 || name_len - pos < out_len ||
                memcmp(&name[pos], out, out_len) != 0) {
            return false;
        }
        pos += out_len;
    }
    return pos == name_len;
}

jkii_parse_err_t jkii_find_name(
        const char* json_string,
        size_t json_string_len,
        const char* name,
        size_t name_len,
        jkii_member_t* out_member)
{
    _jkii_value_t root;
    size_t start = 0;
    size_t next = 0;
    JSONPair_t pair;
    int64_t integer = 0;
    jkii_primitive_err_t integer_res = JKII_PRIMITIVE_ERR_INVALID;

    M_JKII_ASSERT(json_string != NULL);
    M_JKII_ASSERT(name != NULL || name_len == 0);
    M_JKII_ASSERT(out_member != NULL);

    jkii_parse_err_t res = _jkii_root(json_string, json_string_len, &root);
    if (res != JKII_ERR_OK) {
        return res;
    }
    if (root.type != JSONObject) {
        return JKII_ERR_ROOT_TYPE;
    }
    while (JSON_Iterate(root.ptr, root.len, &start, &next, &pair) == JSONSuccess) {
        if (_jkii_key_is(pair.key, pair.keyLength, name != NULL ? name : "", name_len)) {
            out_member->key = pair.key;
            out_member->key_length = pair.keyLength;
            out_member->value = pair.value;
            out_member->value_length = pair.valueLength;
            out_member->type = _jkii_to_jkii_field_type(pair.jsonType, pair.value,
                    pair.valueLength, &integer, &integer_res);
            return JKII_ERR_OK;
        }
    }
    return JKII_ERR_NOT_FOUND;
}

/* The value a path leads to, for the finding readers: JKII_FIELD_ERR_OK with
 * it in found, or why there is none to read. out_member, if not NULL, is set
 * to it whenever there is one, null included. */
static jkii_field_err_t _jkii_find_value(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        unsigned int flags,
        jkii_member_t* found,
        jkii_member_t* out_member)
{
    switch (jkii_find_member(json_string, json_string_len, path, found)) {
        case JKII_ERR_OK:
            break;
        case JKII_ERR_NOT_FOUND:
            return JKII_FIELD_ERR_NOT_FOUND;
        default:
            return JKII_FIELD_ERR_INVALID_INPUT;
    }
    if (out_member != NULL) {
        *out_member = *found;
    }
    if (found->type == JKII_FIELD_TYPE_NULL) {
        return (flags & JKII_READ_NULL_AS_ABSENT) != 0 ?
            JKII_FIELD_ERR_NOT_FOUND : JKII_FIELD_ERR_NULL;
    }
    return JKII_FIELD_ERR_OK;
}

/* found, or with JKII_READ_UNQUOTE what its string holds. */
static const jkii_member_t* _jkii_read_as(
        const jkii_member_t* found,
        unsigned int flags,
        jkii_member_t* unquoted)
{
    return (flags & JKII_READ_UNQUOTE) != 0 ? jkii_member_unquote(found, unquoted) : found;
}

jkii_field_err_t jkii_find_integer(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        int64_t min,
        int64_t max,
        unsigned int flags,
        int64_t* out_value,
        jkii_member_t* out_member)
{
    jkii_member_t found;
    jkii_member_t unquoted;
    const jkii_member_t* value = NULL;
    jkii_field_err_t res = _jkii_find_value(json_string, json_string_len, path, flags,
            &found, out_member);
    if (res != JKII_FIELD_ERR_OK) {
        return res;
    }
    value = _jkii_read_as(&found, flags, &unquoted);
    if ((flags & JKII_READ_INTEGER_AS_WRITTEN) != 0 &&
            value->type != JKII_FIELD_TYPE_INTEGER && value->type != JKII_FIELD_TYPE_LONG) {
        return JKII_FIELD_ERR_TYPE_MISMATCH;
    }
    return jkii_member_integer(value, min, max, out_value);
}

jkii_field_err_t jkii_find_double(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        unsigned int flags,
        double* out_value,
        jkii_member_t* out_member)
{
    jkii_member_t found;
    jkii_member_t unquoted;
    jkii_field_err_t res = _jkii_find_value(json_string, json_string_len, path, flags,
            &found, out_member);
    if (res != JKII_FIELD_ERR_OK) {
        return res;
    }
    return jkii_member_double(_jkii_read_as(&found, flags, &unquoted), out_value);
}

jkii_field_err_t jkii_find_boolean(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        unsigned int flags,
        jkii_boolean_t* out_value,
        jkii_member_t* out_member)
{
    jkii_member_t found;
    jkii_member_t unquoted;
    jkii_field_err_t res = _jkii_find_value(json_string, json_string_len, path, flags,
            &found, out_member);
    if (res != JKII_FIELD_ERR_OK) {
        return res;
    }
    return jkii_member_boolean(_jkii_read_as(&found, flags, &unquoted), out_value);
}

jkii_field_err_t jkii_find_string(
        const char* json_string,
        size_t json_string_len,
        const char* path,
        unsigned int flags,
        char* buff,
        size_t buff_size,
        jkii_member_t* out_member)
{
    jkii_member_t found;
    jkii_field_err_t res;
    if (buff_size > 0) {
        buff[0] = '\0';
    }
    res = _jkii_find_value(json_string, json_string_len, path, flags, &found, out_member);
    if (res != JKII_FIELD_ERR_OK) {
        return res;
    }
    return jkii_member_string(&found, buff, buff_size);
}

