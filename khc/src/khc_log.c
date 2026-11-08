#include "khc.h"
#include "khc_impl.h"

#include <string.h>

void khc_set_logger(khc* khc, ebisu_logger_t* logger)
{
    khc->_logger = logger;
}

#ifndef EBISU_NO_LOG
// Whether the name of length len is name, ignoring case, as header names are.
static bool _is_name(const char* line, size_t len, const char* name)
{
    size_t i;
    if (strlen(name) != len) {
        return false;
    }
    for (i = 0; i < len; ++i) {
        char c = line[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != name[i]) {
            return false;
        }
    }
    return true;
}

void _khc_log_header(khc* khc, char direction, const char* line, size_t line_length)
{
    const char* colon;
    size_t name_length;
    size_t value_start;
    size_t scheme_length;
    if (khc->_logger == NULL) {
        return;
    }
    colon = memchr(line, ':', line_length);
    if (colon == NULL) {
        // The status line, or what isn't a header.
        _khc_log(khc, EBISU_LOG_DEBUG, "%c %.*s", direction, (int)line_length, line);
        return;
    }
    name_length = (size_t)(colon - line);
    if (_is_name(line, name_length, "cookie") || _is_name(line, name_length, "set-cookie")) {
        _khc_log(khc, EBISU_LOG_DEBUG, "%c %.*s: ***", direction, (int)name_length, line);
        return;
    }
    if (!_is_name(line, name_length, "authorization") &&
            !_is_name(line, name_length, "proxy-authorization")) {
        _khc_log(khc, EBISU_LOG_DEBUG, "%c %.*s", direction, (int)line_length, line);
        return;
    }
    // The scheme, such as Bearer or Basic, says what kind of credentials they
    // were, which they themselves mustn't.
    value_start = name_length + 1;
    while (value_start < line_length && (line[value_start] == ' ' || line[value_start] == '\t')) {
        ++value_start;
    }
    scheme_length = 0;
    while (value_start + scheme_length < line_length &&
            line[value_start + scheme_length] != ' ' &&
            line[value_start + scheme_length] != '\t') {
        ++scheme_length;
    }
    if (value_start + scheme_length == line_length) {
        // A value of one word, which may be the credentials themselves.
        scheme_length = 0;
    }
    _khc_log(khc, EBISU_LOG_DEBUG, "%c %.*s: %.*s%s***", direction, (int)name_length, line,
            (int)scheme_length, line + value_start, scheme_length > 0 ? " " : "");
}

void _khc_log_lines(khc* khc, char direction, const char* lines)
{
    const char* end;
    if (khc->_logger == NULL) {
        return;
    }
    while ((end = strstr(lines, "\r\n")) != NULL && end != lines) {
        _khc_log_header(khc, direction, lines, (size_t)(end - lines));
        lines = end + 2;
    }
}

const char* _khc_code_name(khc_code code)
{
    switch (code) {
    case KHC_ERR_OK: return "KHC_ERR_OK";
    case KHC_ERR_SOCK_CONNECT: return "KHC_ERR_SOCK_CONNECT";
    case KHC_ERR_SOCK_CLOSE: return "KHC_ERR_SOCK_CLOSE";
    case KHC_ERR_SOCK_SEND: return "KHC_ERR_SOCK_SEND";
    case KHC_ERR_SOCK_RECV: return "KHC_ERR_SOCK_RECV";
    case KHC_ERR_HEADER_CALLBACK: return "KHC_ERR_HEADER_CALLBACK";
    case KHC_ERR_WRITE_CALLBACK: return "KHC_ERR_WRITE_CALLBACK";
    case KHC_ERR_ALLOCATION: return "KHC_ERR_ALLOCATION";
    case KHC_ERR_TOO_LARGE_DATA: return "KHC_ERR_TOO_LARGE_DATA";
    case KHC_ERR_FAIL: return "KHC_ERR_FAIL";
    case KHC_ERR_READ_CALLBACK: return "KHC_ERR_READ_CALLBACK";
    case KHC_ERR_TIMEOUT: return "KHC_ERR_TIMEOUT";
    case KHC_ERR_CANCELLED: return "KHC_ERR_CANCELLED";
    case KHC_ERR_INVALID_RESPONSE: return "KHC_ERR_INVALID_RESPONSE";
    }
    return "an unknown khc_code";
}
#endif
