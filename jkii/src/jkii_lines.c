#include "jkii_lines.h"

#include <string.h>

void jkii_lines_init(jkii_lines_t* r, char* buff, size_t buff_size)
{
    memset(r, 0, sizeof(*r));
    r->_buff = buff;
    r->_buff_size = buff != NULL ? buff_size : 0;
}

/* Whether the n bytes at s are all JSON whitespace other than '\n'. */
static int _blank(const char* s, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (s[i] != ' ' && s[i] != '\t' && s[i] != '\r') {
            return 0;
        }
    }
    return 1;
}

/* A record given out at the last call is dropped at the next. */
static void _drop_returned(jkii_lines_t* r)
{
    if (r->_returned) {
        r->_len = 0;
        r->_returned = 0;
    }
}

/* The record in the buffer, without a '\r' at its end, terminated, unless it
 * is blank. */
static int _take_record(jkii_lines_t* r, const char** out_line, size_t* out_line_len)
{
    size_t n = r->_len;
    if (n > 0 && r->_buff[n - 1] == '\r') {
        --n;
    }
    if (_blank(r->_buff, n)) {
        r->_len = 0;
        return 0;
    }
    r->_buff[n] = '\0';
    r->_returned = 1;
    *out_line = r->_buff;
    *out_line_len = n;
    return 1;
}

jkii_lines_err_t jkii_lines_feed(jkii_lines_t* r, const char* data, size_t len,
        size_t* out_consumed, const char** out_line, size_t* out_line_len)
{
    *out_consumed = 0;
    *out_line = NULL;
    *out_line_len = 0;
    _drop_returned(r);
    size_t i = 0;
    while (i < len) {
        const char* newline = (const char*)memchr(&data[i], '\n', len - i);
        size_t end = newline != NULL ? (size_t)(newline - data) : len;
        size_t part = end - i;
        if (!r->_skipping && part > 0) {
            /* Room for the terminator too. */
            size_t room = r->_buff_size > 0 ? r->_buff_size - 1 - r->_len : 0;
            if (part > room) {
                r->_skipping = 1;
                r->_len = 0;
            } else {
                memcpy(&r->_buff[r->_len], &data[i], part);
                r->_len += part;
            }
        }
        if (newline == NULL) {
            break;
        }
        /* The '\n' ends the record, and is consumed with it. */
        i = end + 1;
        if (r->_skipping) {
            r->_skipping = 0;
            r->_len = 0;
            *out_consumed = i;
            return JKII_LINES_TOO_LONG;
        }
        if (_take_record(r, out_line, out_line_len)) {
            *out_consumed = i;
            return JKII_LINES_RECORD;
        }
    }
    *out_consumed = len;
    return JKII_LINES_NEED_MORE;
}

jkii_lines_err_t jkii_lines_finish(jkii_lines_t* r, const char** out_line,
        size_t* out_line_len)
{
    *out_line = NULL;
    *out_line_len = 0;
    _drop_returned(r);
    if (r->_skipping) {
        r->_skipping = 0;
        r->_len = 0;
        return JKII_LINES_TOO_LONG;
    }
    if (_take_record(r, out_line, out_line_len)) {
        return JKII_LINES_TRUNCATED;
    }
    return JKII_LINES_END;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
