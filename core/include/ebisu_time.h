/**
 * @file ebisu_time.h
 * @brief UTC dates and times to seconds since the epoch, and back.
 */
#ifndef _EBISU_TIME
#define _EBISU_TIME

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Dates are of the Gregorian calendar, from year 1 to 9999, before its
 * adoption too, as ISO 8601 and TLS stacks have them. The conversions take
 * integer arithmetic only, without the C library's time functions: C99 has no
 * timegm(), gmtime() isn't thread-safe, and an embedded C library may have
 * neither.
 */

/**
 * \brief A UTC date and time, as ebisu_time_to_civil() gives it.
 */
typedef struct ebisu_civil_time_t {
    /** \brief 1 to 9999. */
    int year;
    /** \brief 1 to 12. */
    int month;
    /** \brief 1 to the length of the month. */
    int day;
    /** \brief 0 to 23. */
    int hour;
    /** \brief 0 to 59. */
    int minute;
    /** \brief 0 to 59. */
    int second;
    /** \brief The day of the week, 0 for Sunday to 6 for Saturday. */
    int weekday;
} ebisu_civil_time_t;

/**
 * \brief Converts a UTC date and time to seconds since the epoch.
 *
 * As a TLS stack gives a certificate's validity dates, for
 * ebisu_clock_check_validity() for instance.
 *
 * \param [in] year 1 to 9999.
 * \param [in] month 1 to 12.
 * \param [in] day 1 to the length of the month.
 * \param [in] hour 0 to 23.
 * \param [in] minute 0 to 59.
 * \param [in] second 0 to 59, or 60 at 23:59 for a leap second, which is
 * the same second as 00:00:00 of the next day: seconds since the epoch count
 * no leap seconds.
 * \param [out] out_seconds Seconds since 1970-01-01T00:00:00Z, negative before
 * it. Written only when converted.
 * \return true if converted, false if a field is out of range.
 */
bool ebisu_time_from_civil(
        int year,
        int month,
        int day,
        int hour,
        int minute,
        int second,
        int64_t* out_seconds);

/**
 * \brief Converts seconds since the epoch to a UTC date and time, as gmtime()
 * does.
 *
 * \param [in] seconds Seconds since 1970-01-01T00:00:00Z, negative before it:
 * from -62135596800, 0001-01-01T00:00:00Z, to 253402300799,
 * 9999-12-31T23:59:59Z.
 * \param [out] out The date and time. Written only when converted.
 * \return true if converted, false if seconds is out of that range.
 */
bool ebisu_time_to_civil(int64_t seconds, ebisu_civil_time_t* out);

/**
 * \brief Whether a year is a leap year of the Gregorian calendar: one that 4
 * divides, unless 100 does and 400 doesn't. So 2024 and 2000 are, and 2023
 * and 1900 aren't.
 *
 * \param [in] year Any year, as the rule has no range.
 * \return true for a leap year.
 */
bool ebisu_time_is_leap_year(int year);

/**
 * \brief The days in a month of a year of the Gregorian calendar.
 *
 * \param [in] year Any year.
 * \param [in] month 1 to 12.
 * \return 28 to 31, or 0 for a month outside 1 to 12.
 */
int ebisu_time_days_in_month(int year, int month);

/**
 * \brief The size of a buffer that ebisu_time_format_iso8601() can write
 * into: 20 characters, as in 2026-11-06T13:53:21Z, and the terminating NUL.
 */
#define EBISU_TIME_ISO8601_SIZE 21

/**
 * \brief Writes seconds since the epoch as a UTC date and time of ISO 8601,
 * as RFC 3339 has it: 2026-11-06T13:53:21Z.
 *
 * Without the C library's printf() or time functions, as the conversions are.
 *
 * \param [in] seconds Seconds since 1970-01-01T00:00:00Z, in
 * ebisu_time_to_civil()'s range.
 * \param [in] sep What goes between the date and the time: 'T', as ISO 8601
 * has it, or another character but NUL, such as ' ', which RFC 3339 allows for
 * a person to read, as in a log line.
 * \param [out] buff The date and time, terminated. On failure, "" if size isn't
 * 0.
 * \param [in] size Its size, at least #EBISU_TIME_ISO8601_SIZE.
 * \return true if written, false if seconds is out of range, size is less than
 * #EBISU_TIME_ISO8601_SIZE, or sep is NUL.
 */
bool ebisu_time_format_iso8601(int64_t seconds, char sep, char* buff, size_t size);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_TIME */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
