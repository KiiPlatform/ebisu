/**
 * @file ebisu_time.h
 * @brief UTC dates and times to seconds since the epoch, and back.
 */
#ifndef _EBISU_TIME
#define _EBISU_TIME

#include <stdbool.h>
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

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_TIME */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
