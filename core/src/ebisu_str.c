#include "ebisu_str.h"

#include <stdarg.h>
#include <string.h>

static const char _hex_digits[] = "0123456789abcdef";

/* 10 to the power of 0 to EBISU_FIXED_MAX_DECIMALS. */
static const uint64_t _pow10[EBISU_FIXED_MAX_DECIMALS + 1] = {
    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL,
    10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL,
    100000000000ULL, 1000000000000ULL, 10000000000000ULL,
    100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL,
    100000000000000000ULL, 1000000000000000000ULL
};

size_t ebisu_strlcpy(char* dst, const char* src, size_t size)
{
    size_t len = src != NULL ? strlen(src) : 0;
    if (size > 0) {
        size_t n = len < size ? len : size - 1;
        if (n > 0) {
            memcpy(dst, src, n);
        }
        dst[n] = '\0';
    }
    return len;
}

void ebisu_strbuf_init(ebisu_strbuf_t* sb, char* buff, size_t size)
{
    sb->buff = buff;
    sb->size = size;
    sb->len = 0;
    sb->overflow = false;
    if (size > 0) {
        buff[0] = '\0';
    }
}

void ebisu_strbuf_append_n(ebisu_strbuf_t* sb, const char* s, size_t n)
{
    if (sb->overflow || n == 0) {
        return;
    }
    /* len < size whenever size > 0, so size - len - 1 doesn't wrap; with size
       0 nothing fits. */
    if (sb->size == 0 || n > sb->size - sb->len - 1) {
        sb->overflow = true;
        return;
    }
    memcpy(&sb->buff[sb->len], s, n);
    sb->len += n;
    sb->buff[sb->len] = '\0';
}

void ebisu_strbuf_append(ebisu_strbuf_t* sb, const char* s)
{
    if (s != NULL) {
        ebisu_strbuf_append_n(sb, s, strlen(s));
    }
}

void ebisu_strbuf_append_char(ebisu_strbuf_t* sb, char c)
{
    ebisu_strbuf_append_n(sb, &c, 1);
}

/* Writes v's decimal digits, at least min_digits of them, at the end of
   buff[size], and returns where they start. */
static char* _udigits(char* buff, size_t size, uint64_t v, unsigned int min_digits)
{
    char* p = &buff[size];
    unsigned int written = 0;
    do {
        *--p = (char)('0' + (int)(v % 10));
        v /= 10;
        ++written;
    } while (v > 0 || written < min_digits);
    return p;
}

void ebisu_strbuf_append_uint(ebisu_strbuf_t* sb, uint64_t v)
{
    char digits[20];
    char* p = _udigits(digits, sizeof(digits), v, 1);
    ebisu_strbuf_append_n(sb, p, (size_t)(&digits[sizeof(digits)] - p));
}

/* The magnitude of v, which -v can't give for INT64_MIN. */
static uint64_t _magnitude(int64_t v)
{
    return v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
}

void ebisu_strbuf_append_int(ebisu_strbuf_t* sb, int64_t v)
{
    char digits[21];
    char* p = _udigits(digits, sizeof(digits), _magnitude(v), 1);
    if (v < 0) {
        *--p = '-';
    }
    ebisu_strbuf_append_n(sb, p, (size_t)(&digits[sizeof(digits)] - p));
}

void ebisu_strbuf_append_hex(ebisu_strbuf_t* sb, uint64_t v, unsigned int min_digits)
{
    char digits[16];
    char* p = &digits[sizeof(digits)];
    unsigned int written = 0;
    if (min_digits > sizeof(digits)) {
        min_digits = sizeof(digits);
    }
    do {
        *--p = _hex_digits[v & 0xf];
        v >>= 4;
        ++written;
    } while (v > 0 || written < min_digits);
    ebisu_strbuf_append_n(sb, p, (size_t)(&digits[sizeof(digits)] - p));
}

void ebisu_strbuf_append_hex_bytes(ebisu_strbuf_t* sb, const uint8_t* bytes, size_t n)
{
    size_t i;
    if (sb->overflow || n == 0) {
        return;
    }
    /* As in ebisu_strbuf_append_n(), for two digits a byte, without 2 * n,
       which could wrap. */
    if (sb->size == 0 || n > (sb->size - sb->len - 1) / 2) {
        sb->overflow = true;
        return;
    }
    for (i = 0; i < n; ++i) {
        sb->buff[sb->len++] = _hex_digits[bytes[i] >> 4];
        sb->buff[sb->len++] = _hex_digits[bytes[i] & 0xf];
    }
    sb->buff[sb->len] = '\0';
}

void ebisu_strbuf_append_fixed(ebisu_strbuf_t* sb, int64_t v, unsigned int decimals)
{
    /* Sign, 19 integer digits at most, the point and the decimals. */
    char digits[2 + 19 + EBISU_FIXED_MAX_DECIMALS];
    char* end = &digits[sizeof(digits)];
    char* p;
    uint64_t magnitude = _magnitude(v);

    if (decimals > EBISU_FIXED_MAX_DECIMALS) {
        sb->overflow = true;
        return;
    }
    if (decimals == 0) {
        ebisu_strbuf_append_int(sb, v);
        return;
    }
    p = _udigits(digits, sizeof(digits), magnitude % _pow10[decimals], decimals);
    *--p = '.';
    p = _udigits(digits, (size_t)(p - digits), magnitude / _pow10[decimals], 1);
    if (v < 0) {
        *--p = '-';
    }
    ebisu_strbuf_append_n(sb, p, (size_t)(end - p));
}

char* ebisu_strbuf_room(ebisu_strbuf_t* sb, size_t* out_size)
{
    if (sb->overflow || sb->size == 0) {
        *out_size = 0;
        return NULL;
    }
    *out_size = sb->size - sb->len;
    return &sb->buff[sb->len];
}

void ebisu_strbuf_commit(ebisu_strbuf_t* sb, size_t n)
{
    if (sb->overflow) {
        return;
    }
    /* As in ebisu_strbuf_append_n(): the room is size - len bytes, the NUL
       takes one of them, and with size 0 there is none. */
    if (n > 0 && (sb->size == 0 || n > sb->size - sb->len - 1)) {
        sb->overflow = true;
    } else {
        sb->len += n;
    }
    /* The writer may have left anything there, its own NUL or none. */
    if (sb->size > 0) {
        sb->buff[sb->len] = '\0';
    }
}

void ebisu_strbuf_appendf(ebisu_strbuf_t* sb, const char* fmt, ...)
{
    size_t start = sb->len;
    va_list args;
    if (sb->overflow) {
        return;
    }
    va_start(args, fmt);
    while (*fmt != '\0' && !sb->overflow) {
        const char* percent = strchr(fmt, '%');
        if (percent == NULL) {
            ebisu_strbuf_append(sb, fmt);
            break;
        }
        ebisu_strbuf_append_n(sb, fmt, (size_t)(percent - fmt));
        if (percent[1] == 's') {
            ebisu_strbuf_append(sb, va_arg(args, const char*));
            fmt = percent + 2;
        } else if (percent[1] == '.' && percent[2] == '*' && percent[3] == 's') {
            int precision = va_arg(args, int);
            const char* s = va_arg(args, const char*);
            if (precision < 0) {
                ebisu_strbuf_append(sb, s);
            } else if (s != NULL) {
                const char* nul = memchr(s, '\0', (size_t)precision);
                ebisu_strbuf_append_n(sb, s, nul != NULL ? (size_t)(nul - s) : (size_t)precision);
            }
            fmt = percent + 4;
        } else if (percent[1] == '%') {
            ebisu_strbuf_append_char(sb, '%');
            fmt = percent + 2;
        } else {
            /* A conversion this doesn't write, or a '%' that ends the format. */
            sb->overflow = true;
        }
    }
    va_end(args);
    if (sb->overflow && sb->size > 0) {
        sb->len = start;
        sb->buff[start] = '\0';
    }
}

bool ebisu_strbuf_ok(const ebisu_strbuf_t* sb)
{
    return !sb->overflow;
}

/* Value of a digit in base 10 or 16, or -1. */
static int _digit(char c, unsigned int base)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (base == 16) {
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
    }
    return -1;
}

/* Parses digits in base into *out, up to max. Every byte is checked to be a
   digit before any range is, so that a span that isn't a number is INVALID
   whatever its length. */
static ebisu_parse_err_t _parse_digits(const char* s, size_t len, unsigned int base,
        uint64_t max, uint64_t* out)
{
    uint64_t value = 0;
    bool range = false;
    size_t i;

    if (len == 0) {
        return EBISU_PARSE_EMPTY;
    }
    for (i = 0; i < len; ++i) {
        if (_digit(s[i], base) < 0) {
            return EBISU_PARSE_INVALID;
        }
    }
    for (i = 0; i < len && !range; ++i) {
        uint64_t d = (uint64_t)_digit(s[i], base);
        if (d > max || value > (max - d) / base) {
            range = true;
        } else {
            value = value * base + d;
        }
    }
    if (range) {
        return EBISU_PARSE_RANGE;
    }
    *out = value;
    return EBISU_PARSE_OK;
}

ebisu_parse_err_t ebisu_parse_uint(const char* s, size_t len, uint64_t max, uint64_t* out)
{
    return _parse_digits(s, len, 10, max, out);
}

ebisu_parse_err_t ebisu_parse_hex(const char* s, size_t len, uint64_t max, uint64_t* out)
{
    return _parse_digits(s, len, 16, max, out);
}

ebisu_parse_err_t ebisu_parse_hex_bytes(const char* s, size_t len, uint8_t* out,
        size_t out_size, size_t* out_len)
{
    size_t i;

    if (len == 0) {
        return EBISU_PARSE_EMPTY;
    }
    /* Every byte is checked before the room is, as in _parse_digits(). */
    if (len % 2 != 0) {
        return EBISU_PARSE_INVALID;
    }
    for (i = 0; i < len; ++i) {
        if (_digit(s[i], 16) < 0) {
            return EBISU_PARSE_INVALID;
        }
    }
    if (len / 2 > out_size) {
        return EBISU_PARSE_RANGE;
    }
    for (i = 0; i < len / 2; ++i) {
        out[i] = (uint8_t)(_digit(s[2 * i], 16) * 16 + _digit(s[2 * i + 1], 16));
    }
    *out_len = len / 2;
    return EBISU_PARSE_OK;
}

/* Takes a leading '+' or '-' off s. */
static bool _take_sign(const char** s, size_t* len, bool* negative)
{
    *negative = false;
    if (*len > 0 && (**s == '+' || **s == '-')) {
        *negative = **s == '-';
        ++*s;
        --*len;
        return true;
    }
    return false;
}

/* The signed value of a magnitude, within [min, max]. */
static ebisu_parse_err_t _signed_value(uint64_t magnitude, bool negative,
        int64_t min, int64_t max, int64_t* out)
{
    int64_t value;
    if (negative) {
        /* INT64_MIN's magnitude is one more than INT64_MAX. */
        if (magnitude > (uint64_t)INT64_MAX + 1) {
            return EBISU_PARSE_RANGE;
        }
        value = magnitude == (uint64_t)INT64_MAX + 1 ? INT64_MIN : -(int64_t)magnitude;
    } else {
        if (magnitude > (uint64_t)INT64_MAX) {
            return EBISU_PARSE_RANGE;
        }
        value = (int64_t)magnitude;
    }
    if (value < min || value > max) {
        return EBISU_PARSE_RANGE;
    }
    *out = value;
    return EBISU_PARSE_OK;
}

ebisu_parse_err_t ebisu_parse_int(const char* s, size_t len, int64_t min, int64_t max, int64_t* out)
{
    bool negative;
    uint64_t magnitude;
    ebisu_parse_err_t err;

    if (len == 0) {
        return EBISU_PARSE_EMPTY;
    }
    if (_take_sign(&s, &len, &negative) && len == 0) {
        return EBISU_PARSE_INVALID;
    }
    err = _parse_digits(s, len, 10, (uint64_t)INT64_MAX + 1, &magnitude);
    if (err != EBISU_PARSE_OK) {
        return err;
    }
    return _signed_value(magnitude, negative, min, max, out);
}

ebisu_parse_err_t ebisu_parse_fixed(const char* s, size_t len, unsigned int decimals,
        int64_t min, int64_t max, int64_t* out)
{
    /* The magnitude of INT64_MIN, the largest a scaled value can have. */
    const uint64_t limit = (uint64_t)INT64_MAX + 1;
    bool negative;
    const char* point;
    size_t int_len;
    size_t frac_len;
    size_t i;
    uint64_t scale;
    uint64_t int_part;
    uint64_t frac_part = 0;

    if (len == 0) {
        return EBISU_PARSE_EMPTY;
    }
    if (_take_sign(&s, &len, &negative) && len == 0) {
        return EBISU_PARSE_INVALID;
    }
    point = (const char*)memchr(s, '.', len);
    int_len = point != NULL ? (size_t)(point - s) : len;
    frac_len = point != NULL ? len - int_len - 1 : 0;
    /* Digits on both sides of one '.', checked before any range is. */
    for (i = 0; i < len; ++i) {
        if (i != int_len && _digit(s[i], 10) < 0) {
            return EBISU_PARSE_INVALID;
        }
    }
    if (int_len == 0 || (point != NULL && frac_len == 0)) {
        return EBISU_PARSE_INVALID;
    }
    if (decimals > EBISU_FIXED_MAX_DECIMALS) {
        return EBISU_PARSE_RANGE;
    }
    if (frac_len > decimals) {
        return EBISU_PARSE_INVALID;
    }
    scale = _pow10[decimals];
    if (_parse_digits(s, int_len, 10, limit / scale, &int_part) != EBISU_PARSE_OK) {
        return EBISU_PARSE_RANGE;
    }
    if (frac_len > 0) {
        /* Fewer than decimals digits, so it is below scale and parses. */
        (void)_parse_digits(point + 1, frac_len, 10, scale - 1, &frac_part);
        frac_part *= _pow10[decimals - frac_len];
    }
    if (int_part > (limit - frac_part) / scale) {
        return EBISU_PARSE_RANGE;
    }
    return _signed_value(int_part * scale + frac_part, negative, min, max, out);
}

ebisu_span_t ebisu_span_from_str(const char* s)
{
    ebisu_span_t span;
    span.ptr = s != NULL ? s : "";
    span.len = strlen(span.ptr);
    return span;
}

bool ebisu_span_next(ebisu_span_t* rest, char delim, ebisu_span_t* field)
{
    const char* found;
    if (rest->ptr == NULL) {
        return false;
    }
    found = rest->len > 0 ? (const char*)memchr(rest->ptr, delim, rest->len) : NULL;
    field->ptr = rest->ptr;
    if (found != NULL) {
        field->len = (size_t)(found - rest->ptr);
        rest->len -= field->len + 1;
        rest->ptr = found + 1;
    } else {
        field->len = rest->len;
        rest->ptr = NULL;
        rest->len = 0;
    }
    return true;
}

static bool _is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

ebisu_span_t ebisu_span_trim(ebisu_span_t s)
{
    while (s.len > 0 && _is_space(s.ptr[0])) {
        ++s.ptr;
        --s.len;
    }
    while (s.len > 0 && _is_space(s.ptr[s.len - 1])) {
        --s.len;
    }
    return s;
}

bool ebisu_span_eq(ebisu_span_t s, const char* literal)
{
    size_t n = literal != NULL ? strlen(literal) : 0;
    if (s.len != n) {
        return false;
    }
    return n == 0 || memcmp(s.ptr, literal, n) == 0;
}

/* An ASCII letter in lower case, and any other byte as it is. */
static char _ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool ebisu_span_eq_nocase(ebisu_span_t s, const char* literal)
{
    size_t n = literal != NULL ? strlen(literal) : 0;
    size_t i;
    if (s.len != n) {
        return false;
    }
    for (i = 0; i < n; ++i) {
        if (_ascii_lower(s.ptr[i]) != _ascii_lower(literal[i])) {
            return false;
        }
    }
    return true;
}

bool ebisu_span_find(ebisu_span_t s, const char* needle, size_t* out_offset)
{
    size_t n = needle != NULL ? strlen(needle) : 0;
    size_t i;
    if (n == 0) {
        *out_offset = 0;
        return true;
    }
    if (s.len < n) {
        return false;
    }
    /* Where the needle's first byte is, among the places it could start, then
       whether the rest follows. */
    for (i = 0; i <= s.len - n; ++i) {
        const char* first = (const char*)memchr(&s.ptr[i], needle[0], s.len - n + 1 - i);
        if (first == NULL) {
            return false;
        }
        i = (size_t)(first - s.ptr);
        if (memcmp(first, needle, n) == 0) {
            *out_offset = i;
            return true;
        }
    }
    return false;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
