#include "jkii_writer.h"

#include <string.h>

/* What _levels holds for each open container. */
#define LEVEL_OBJECT 0x1
#define LEVEL_HAS_MEMBER 0x2
#define LEVEL_AFTER_KEY 0x4

void jkii_writer_init_buff(jkii_writer_t* w, char* buff, size_t buff_size)
{
    memset(w, 0, sizeof(*w));
    w->_buff = buff;
    w->_buff_size = buff != NULL ? buff_size : 0;
    w->_err = JKII_WRITE_OK;
    if (w->_buff_size > 0) {
        w->_buff[0] = '\0';
    }
}

void jkii_writer_init_cb(jkii_writer_t* w, char* buff, size_t buff_size,
        JKII_CB_WRITE cb, void* userdata)
{
    memset(w, 0, sizeof(*w));
    w->_buff = buff;
    w->_buff_size = buff_size;
    w->_cb = cb;
    w->_userdata = userdata;
    w->_err = JKII_WRITE_OK;
    if (buff == NULL || buff_size == 0 || cb == NULL) {
        w->_err = JKII_WRITE_ERR_CALLBACK;
    }
}

/* An error after which nothing more is written, nor counted. */
static int _stopped(const jkii_writer_t* w)
{
    return w->_err != JKII_WRITE_OK && w->_err != JKII_WRITE_ERR_TOO_LARGE;
}

static jkii_write_err_t _fail(jkii_writer_t* w, jkii_write_err_t err)
{
    if (!_stopped(w)) {
        w->_err = err;
    }
    return w->_err;
}

static void _flush(jkii_writer_t* w)
{
    if (w->_used > 0 && !_stopped(w)) {
        size_t taken = w->_cb(w->_buff, w->_used, w->_userdata);
        if (taken != w->_used) {
            _fail(w, JKII_WRITE_ERR_CALLBACK);
        }
    }
    w->_used = 0;
}

/* Counts what is written, and keeps what fits, or gives it to the callback. */
static void _put(jkii_writer_t* w, const char* s, size_t n)
{
    /* Nothing to copy, from what may be NULL. */
    if (_stopped(w) || n == 0) {
        return;
    }
    w->_total += n;
    if (w->_cb != NULL) {
        while (n > 0 && !_stopped(w)) {
            size_t room = w->_buff_size - w->_used;
            size_t part = n < room ? n : room;
            memcpy(&w->_buff[w->_used], s, part);
            w->_used += part;
            s += part;
            n -= part;
            if (w->_used == w->_buff_size) {
                _flush(w);
            }
        }
        return;
    }
    if (w->_buff == NULL || w->_err == JKII_WRITE_ERR_TOO_LARGE) {
        return;
    }
    /* Room for the terminator too. */
    if (n >= w->_buff_size - w->_used) {
        w->_err = JKII_WRITE_ERR_TOO_LARGE;
        return;
    }
    memcpy(&w->_buff[w->_used], s, n);
    w->_used += n;
    w->_buff[w->_used] = '\0';
}

static void _put_str(jkii_writer_t* w, const char* s)
{
    _put(w, s, strlen(s));
}

/* Before a value: where one can go, with a comma before it in an array that
 * has members already. */
static jkii_write_err_t _begin_value(jkii_writer_t* w)
{
    if (_stopped(w)) {
        return w->_err;
    }
    if (w->_depth == 0) {
        return w->_top_done ? _fail(w, JKII_WRITE_ERR_STRUCTURE) : w->_err;
    }
    unsigned char* level = &w->_levels[w->_depth - 1];
    if (*level & LEVEL_OBJECT) {
        if (!(*level & LEVEL_AFTER_KEY)) {
            return _fail(w, JKII_WRITE_ERR_STRUCTURE);
        }
        *level &= (unsigned char)~LEVEL_AFTER_KEY;
    } else {
        if (*level & LEVEL_HAS_MEMBER) {
            _put(w, ",", 1);
        }
        *level |= LEVEL_HAS_MEMBER;
    }
    return w->_err;
}

/* After a value, or the end of a container: at the top, the JSON is whole. */
static jkii_write_err_t _end_value(jkii_writer_t* w)
{
    if (w->_depth == 0 && !_stopped(w)) {
        w->_top_done = 1;
    }
    return w->_err;
}

static jkii_write_err_t _start(jkii_writer_t* w, unsigned char kind, const char* open)
{
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    if (w->_depth == JKII_WRITER_MAX_DEPTH) {
        return _fail(w, JKII_WRITE_ERR_TOO_DEEP);
    }
    w->_levels[w->_depth++] = kind;
    _put(w, open, 1);
    return w->_err;
}

static jkii_write_err_t _end(jkii_writer_t* w, unsigned char kind, const char* close)
{
    if (_stopped(w)) {
        return w->_err;
    }
    if (w->_depth == 0) {
        return _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    unsigned char level = w->_levels[w->_depth - 1];
    if ((level & LEVEL_OBJECT) != kind || (level & LEVEL_AFTER_KEY)) {
        return _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    --w->_depth;
    _put(w, close, 1);
    return _end_value(w);
}

jkii_write_err_t jkii_write_object_start(jkii_writer_t* w)
{
    return _start(w, LEVEL_OBJECT, "{");
}

jkii_write_err_t jkii_write_object_end(jkii_writer_t* w)
{
    return _end(w, LEVEL_OBJECT, "}");
}

jkii_write_err_t jkii_write_array_start(jkii_writer_t* w)
{
    return _start(w, 0, "[");
}

jkii_write_err_t jkii_write_array_end(jkii_writer_t* w)
{
    return _end(w, 0, "]");
}

/* s's n bytes as a JSON string, escaped as jkii_escape_str() escapes. */
static void _put_escaped(jkii_writer_t* w, const char* s, size_t n)
{
    static const char hex_digits[] = "0123456789abcdef";
    _put(w, "\"", 1);
    size_t plain = 0;
    for (size_t i = 0; i < n; ++i) {
        /* Unsigned, so that UTF-8 bytes do not compare below 0x20. */
        unsigned char c = (unsigned char)s[i];
        char esc[7] = { '\\', '\0', '\0', '\0', '\0', '\0', '\0' };
        size_t esc_len = 2;
        switch (c) {
            case '"':
            case '/':
            case '\\':
                esc[1] = (char)c;
                break;
            case '\b':
                esc[1] = 'b';
                break;
            case '\f':
                esc[1] = 'f';
                break;
            case '\n':
                esc[1] = 'n';
                break;
            case '\r':
                esc[1] = 'r';
                break;
            case '\t':
                esc[1] = 't';
                break;
            default:
                if (c >= 0x20) {
                    continue;
                }
                memcpy(&esc[1], "u00", 3);
                esc[4] = hex_digits[c >> 4];
                esc[5] = hex_digits[c & 0xf];
                esc_len = 6;
                break;
        }
        /* What came before it went out as it was. */
        _put(w, &s[plain], i - plain);
        _put(w, esc, esc_len);
        plain = i + 1;
    }
    _put(w, &s[plain], n - plain);
    _put(w, "\"", 1);
}

jkii_write_err_t jkii_write_key(jkii_writer_t* w, const char* key)
{
    if (_stopped(w)) {
        return w->_err;
    }
    if (key == NULL) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    if (w->_depth == 0) {
        return _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    unsigned char* level = &w->_levels[w->_depth - 1];
    if (!(*level & LEVEL_OBJECT) || (*level & LEVEL_AFTER_KEY)) {
        return _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    if (*level & LEVEL_HAS_MEMBER) {
        _put(w, ",", 1);
    }
    *level |= LEVEL_HAS_MEMBER | LEVEL_AFTER_KEY;
    _put_escaped(w, key, strlen(key));
    _put(w, ":", 1);
    return w->_err;
}

jkii_write_err_t jkii_write_string_n(jkii_writer_t* w, const char* s, size_t n)
{
    if (s == NULL && n > 0) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    _put_escaped(w, s != NULL ? s : "", n);
    return _end_value(w);
}

jkii_write_err_t jkii_write_string(jkii_writer_t* w, const char* s)
{
    if (s == NULL) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    return jkii_write_string_n(w, s, strlen(s));
}

/* v in decimal into out, which has room for 20 digits, unterminated. Written
 * by hand: printf's conversions for 64 bits are missing from some embedded C
 * libraries. */
static size_t _decimal(uint64_t v, char out[20])
{
    char digits[20];
    size_t len = 0;
    do {
        digits[len++] = (char)('0' + v % 10);
        v /= 10;
    } while (v != 0);
    for (size_t i = 0; i < len; ++i) {
        out[i] = digits[len - 1 - i];
    }
    return len;
}

static jkii_write_err_t _write_number(jkii_writer_t* w, int negative, uint64_t magnitude)
{
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    char digits[20];
    size_t len = _decimal(magnitude, digits);
    if (negative) {
        _put(w, "-", 1);
    }
    _put(w, digits, len);
    return _end_value(w);
}

jkii_write_err_t jkii_write_int(jkii_writer_t* w, int64_t v)
{
    /* -(v + 1) + 1 rather than -v, which overflows for INT64_MIN. */
    uint64_t magnitude = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
    return _write_number(w, v < 0, magnitude);
}

jkii_write_err_t jkii_write_uint(jkii_writer_t* w, uint64_t v)
{
    return _write_number(w, 0, v);
}

jkii_write_err_t jkii_write_double(jkii_writer_t* w, double v, int decimals)
{
    static const double powers_of_ten[] = {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9
    };
    static const uint64_t int_powers_of_ten[] = {
        1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u,
        100000000u, 1000000000u
    };
    /* 2 to the 64. */
    const double limit = 18446744073709551616.0;
    if (_stopped(w)) {
        return w->_err;
    }
    /* NaN is unequal to itself, and an infinity less itself is NaN: neither
     * needs math.h. */
    if (v != v || v - v != v - v || decimals < 0 || decimals > 9) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    double magnitude = v < 0 ? -v : v;
    if (magnitude * powers_of_ten[decimals] >= limit) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    /* The integer part split off first, which is exact, so that the fraction
     * keeps all of the double's precision when it is scaled: 2.675 times 100
     * is 267.5 in double, where 0.675 times 100 is just below 67.5. */
    uint64_t unit = int_powers_of_ten[decimals];
    uint64_t int_part = (uint64_t)magnitude;
    double scaled = (magnitude - (double)int_part) * powers_of_ten[decimals];
    uint64_t fraction = (uint64_t)scaled;
    /* By the fraction rather than by adding 0.5, which rounds itself. */
    if (scaled - (double)fraction >= 0.5) {
        ++fraction;
    }
    if (fraction == unit) {
        fraction = 0;
        ++int_part;
    }
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    char digits[20];
    if (v < 0 && (int_part != 0 || fraction != 0)) {
        _put(w, "-", 1);
    }
    _put(w, digits, _decimal(int_part, digits));
    if (decimals > 0) {
        /* The fraction, with its leading zeros. */
        char fraction_digits[9];
        for (int i = decimals - 1; i >= 0; --i) {
            fraction_digits[i] = (char)('0' + fraction % 10);
            fraction /= 10;
        }
        _put(w, ".", 1);
        _put(w, fraction_digits, (size_t)decimals);
    }
    return _end_value(w);
}

static jkii_write_err_t _write_literal(jkii_writer_t* w, const char* literal)
{
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    _put_str(w, literal);
    return _end_value(w);
}

jkii_write_err_t jkii_write_bool(jkii_writer_t* w, int v)
{
    return _write_literal(w, v ? "true" : "false");
}

jkii_write_err_t jkii_write_null(jkii_writer_t* w)
{
    return _write_literal(w, "null");
}

jkii_write_err_t jkii_write_raw(jkii_writer_t* w, const char* json, size_t n)
{
    if (json == NULL || n == 0) {
        return _fail(w, JKII_WRITE_ERR_VALUE);
    }
    if (_begin_value(w) != JKII_WRITE_OK && _stopped(w)) {
        return w->_err;
    }
    _put(w, json, n);
    return _end_value(w);
}

jkii_write_err_t jkii_write_record_end(jkii_writer_t* w)
{
    if (_stopped(w)) {
        return w->_err;
    }
    if (w->_depth > 0 || !w->_top_done) {
        return _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    _put(w, "\n", 1);
    w->_top_done = 0;
    ++w->_records;
    return w->_err;
}

jkii_write_err_t jkii_writer_finish(jkii_writer_t* w, size_t* out_len)
{
    if (!_stopped(w) && (w->_depth > 0 || (!w->_top_done && w->_records == 0))) {
        _fail(w, JKII_WRITE_ERR_STRUCTURE);
    }
    if (w->_cb != NULL) {
        _flush(w);
    }
    if (out_len != NULL) {
        *out_len = w->_total;
    }
    return w->_err;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
