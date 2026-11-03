#include "khc_http_date.h"

#include <string.h>

#include "ebisu_time.h"

static const char _DAY_NAMES[7][3] = {
    { 'S', 'u', 'n' }, { 'M', 'o', 'n' }, { 'T', 'u', 'e' }, { 'W', 'e', 'd' },
    { 'T', 'h', 'u' }, { 'F', 'r', 'i' }, { 'S', 'a', 't' }
};
/* RFC 850's whole names, in the same order. */
static const char* const _DAY_NAMES_LONG[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char _MONTH_NAMES[12][3] = {
    { 'J', 'a', 'n' }, { 'F', 'e', 'b' }, { 'M', 'a', 'r' }, { 'A', 'p', 'r' },
    { 'M', 'a', 'y' }, { 'J', 'u', 'n' }, { 'J', 'u', 'l' }, { 'A', 'u', 'g' },
    { 'S', 'e', 'p' }, { 'O', 'c', 't' }, { 'N', 'o', 'v' }, { 'D', 'e', 'c' }
};

/* The fields of an HTTP-date, as written: any of them may be out of range, or
 * -1 where the text held no number or name. */
typedef struct {
    int weekday;
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;
} _http_date_t;

/* The index in names of the three letters at s, compared case-sensitively as
 * RFC 9110 asks, or -1. */
static int _find_name(const char* s, const char (*names)[3], int count)
{
    for (int i = 0; i < count; ++i) {
        if (memcmp(s, names[i], 3) == 0) {
            return i;
        }
    }
    return -1;
}

/* The value of the n decimal digits at s, or -1 if any of them is not one. */
static int _parse_digits(const char* s, size_t n)
{
    int value = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return -1;
        }
        value = value * 10 + (s[i] - '0');
    }
    return value;
}

/* The year seconds since the epoch fall in; 1970 for anything earlier. */
static int _year_of(int64_t seconds)
{
    ebisu_civil_time_t civil;
    if (seconds > 0 && ebisu_time_to_civil(seconds, &civil)) {
        return civil.year;
    }
    return 1970;
}

/* "08:49:37" */
static int _parse_time_of_day(const char* s, _http_date_t* d)
{
    if (s[2] != ':' || s[5] != ':') {
        return -1;
    }
    d->hour = _parse_digits(s, 2);
    d->minute = _parse_digits(s + 3, 2);
    d->second = _parse_digits(s + 6, 2);
    return 0;
}

/* GMT, or UTC, which some servers send for it. */
static int _is_gmt(const char* s)
{
    return memcmp(s, "GMT", 3) == 0 || memcmp(s, "UTC", 3) == 0;
}

/* IMF-fixdate, the form servers must send: "Sun, 06 Nov 1994 08:49:37 GMT". */
static int _parse_imf_fixdate(const char* v, size_t len, _http_date_t* d)
{
    if (len != 29 || v[3] != ',' || v[4] != ' ' || v[7] != ' ' ||
            v[11] != ' ' || v[16] != ' ' || v[25] != ' ' || !_is_gmt(v + 26)) {
        return -1;
    }
    d->weekday = _find_name(v, _DAY_NAMES, 7);
    d->day = _parse_digits(v + 5, 2);
    d->month = _find_name(v + 8, _MONTH_NAMES, 12) + 1;
    d->year = _parse_digits(v + 12, 4);
    return _parse_time_of_day(v + 17, d);
}

/* C's asctime(), with no zone since it is always GMT, and the day padded with
 * a space or written with two digits: "Sun Nov  6 08:49:37 1994". */
static int _parse_asctime(const char* v, size_t len, _http_date_t* d)
{
    if (len != 24 || v[3] != ' ' || v[7] != ' ' || v[10] != ' ' || v[19] != ' ') {
        return -1;
    }
    d->weekday = _find_name(v, _DAY_NAMES, 7);
    d->month = _find_name(v + 4, _MONTH_NAMES, 12) + 1;
    d->day = (v[8] == ' ') ? _parse_digits(v + 9, 1) : _parse_digits(v + 8, 2);
    d->year = _parse_digits(v + 20, 4);
    return _parse_time_of_day(v + 11, d);
}

/* RFC 850, with the day's whole name and two digits of the year:
 * "Sunday, 06-Nov-94 08:49:37 GMT". The year is the one with those two digits
 * within 50 years either side of present_year, never more than 50 years ahead
 * of it, as RFC 9110 asks. */
static int _parse_rfc850(const char* v, size_t len, int present_year, _http_date_t* d)
{
    int weekday = -1;
    size_t name_len = 0;
    for (int i = 0; i < 7; ++i) {
        size_t n = strlen(_DAY_NAMES_LONG[i]);
        if (len > n && memcmp(v, _DAY_NAMES_LONG[i], n) == 0 && v[n] == ',') {
            weekday = i;
            name_len = n;
            break;
        }
    }
    /* ", 06-Nov-94 08:49:37 GMT" after the name. */
    if (weekday < 0 || len != name_len + 24) {
        return -1;
    }
    const char* s = v + name_len + 2;
    if (v[name_len + 1] != ' ' || s[2] != '-' || s[6] != '-' || s[9] != ' ' ||
            s[18] != ' ' || !_is_gmt(s + 19)) {
        return -1;
    }
    d->weekday = weekday;
    d->day = _parse_digits(s, 2);
    d->month = _find_name(s + 3, _MONTH_NAMES, 12) + 1;
    int two_digits = _parse_digits(s + 7, 2);
    if (two_digits < 0) {
        return -1;
    }
    d->year = present_year - present_year % 100 + two_digits;
    if (d->year > present_year + 50) {
        d->year -= 100;
    } else if (d->year <= present_year - 50) {
        d->year += 100;
    }
    return _parse_time_of_day(s + 10, d);
}

int64_t _parse_http_date(const char* header, size_t header_len, int64_t floor)
{
    /* RFC 9110 allows no whitespace between a field's name and the colon. */
    static const char name[] = "date:";
    const size_t name_len = sizeof(name) - 1;
    if (header == NULL || header_len < name_len) {
        return -1;
    }
    for (size_t i = 0; i < name_len; ++i) {
        char c = header[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != name[i]) {
            return -1;
        }
    }

    /* The value, without the whitespace around it. */
    const char* v = header + name_len;
    const char* end = header + header_len;
    while (v < end && (*v == ' ' || *v == '\t')) {
        ++v;
    }
    while (end > v && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    size_t len = (size_t)(end - v);

    /* The three forms of RFC 9110, section 5.6.7, which a recipient must all
     * take. Each sets every field when its layout matches. */
    _http_date_t d;
    if (_parse_imf_fixdate(v, len, &d) != 0 &&
            _parse_asctime(v, len, &d) != 0 &&
            _parse_rfc850(v, len, _year_of(floor), &d) != 0) {
        return -2;
    }
    /* The day's midnight as well, for its name, which has to agree with the
     * date: 1970-01-01 was a Thursday. A leap second's date is the next
     * day's midnight. */
    int64_t date = 0;
    int64_t midnight = 0;
    if (d.weekday < 0 || d.year < 1970 ||
            !ebisu_time_from_civil(d.year, d.month, d.day, d.hour, d.minute, d.second, &date) ||
            !ebisu_time_from_civil(d.year, d.month, d.day, 0, 0, 0, &midnight) ||
            (midnight / 86400 + 4) % 7 != d.weekday) {
        return -2;
    }
    return (date < floor) ? -2 : date;
}

void _record_resp_date(khc* khc, const char* line, size_t line_len)
{
    if (line == NULL) {
        return;
    }
    int64_t date = _parse_http_date(line, line_len, khc->_resp_date_floor);
    if (date == -1) {
        return;
    }
    /* A malformed Date, or Dates that disagree, leave the response without
     * one rather than with whichever came last. -2 holds that until the next
     * response's status line. */
    if (date == -2 || (khc->_resp_date != -1 && khc->_resp_date != date)) {
        khc->_resp_date = -2;
    } else {
        khc->_resp_date = date;
    }
}
