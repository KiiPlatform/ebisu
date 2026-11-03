#include "ebisu_time.h"

#include <stddef.h>

/* 0001-01-01T00:00:00Z and 9999-12-31T23:59:59Z. */
#define _FIRST_SECOND (-62135596800LL)
#define _LAST_SECOND 253402300799LL

static bool _is_leap(int year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int _days_in_month(int year, int month)
{
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && _is_leap(year)) {
        return 29;
    }
    return days[month - 1];
}

/* Days from 1970-01-01 to a valid date, from year 1 on, negative before 1970.
 * The year is counted from March, so that a leap day comes last in it.
 * Howard Hinnant's days_from_civil, donated to the public domain, without its
 * handling of years before 0:
 * https://howardhinnant.github.io/date_algorithms.html#days_from_civil */
static int64_t _days_from_civil(int year, int month, int day)
{
    int64_t y = (month <= 2) ? year - 1 : year;
    int64_t m = (month <= 2) ? month + 9 : month - 3;
    int64_t days = y * 365 + y / 4 - y / 100 + y / 400 + (153 * m + 2) / 5 + day - 1;
    /* The same count for 1970-01-01. */
    return days - 719468;
}

bool ebisu_time_from_civil(
        int year,
        int month,
        int day,
        int hour,
        int minute,
        int second,
        int64_t* out_seconds)
{
    /* 60 is a leap second, which only ever ends a UTC day. */
    if (out_seconds == NULL || year < 1 || year > 9999 || month < 1 || month > 12 ||
            day < 1 || day > _days_in_month(year, month) ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
            second < 0 || second > 60 ||
            (second == 60 && (hour != 23 || minute != 59))) {
        return false;
    }
    int64_t days = _days_from_civil(year, month, day);
    *out_seconds = ((days * 24 + hour) * 60 + minute) * 60 + second;
    return true;
}

/* The reverse, Hinnant's civil_from_days, for days from year 1 on: counted
 * from 0000-03-01, which makes them non-negative, in 400-year eras of 146097
 * days. */
bool ebisu_time_to_civil(int64_t seconds, ebisu_civil_time_t* out)
{
    if (out == NULL || seconds < _FIRST_SECOND || seconds > _LAST_SECOND) {
        return false;
    }
    /* Seconds from 0001-01-01 are non-negative, so / and % round down. */
    int64_t from_first = seconds - _FIRST_SECOND;
    int64_t days = from_first / 86400 + (_FIRST_SECOND / 86400);
    int64_t of_day = from_first % 86400;

    int64_t z = days + 719468;
    int64_t era = z / 146097;
    int64_t day_of_era = z - era * 146097;
    int64_t year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 -
            day_of_era / 146096) / 365;
    int64_t day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 -
            year_of_era / 100);
    int64_t m = (5 * day_of_year + 2) / 153;
    int month = (int)(m < 10 ? m + 3 : m - 9);

    out->year = (int)(year_of_era + era * 400 + (month <= 2 ? 1 : 0));
    out->month = month;
    out->day = (int)(day_of_year - (153 * m + 2) / 5 + 1);
    out->hour = (int)(of_day / 3600);
    out->minute = (int)(of_day / 60 % 60);
    out->second = (int)(of_day % 60);
    /* 0001-01-01 was a Monday. */
    out->weekday = (int)((days - _FIRST_SECOND / 86400 + 1) % 7);
    return true;
}
