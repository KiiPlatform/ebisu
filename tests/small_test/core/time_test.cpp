#include <catch.hpp>
#include <ebisu_time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

namespace {

bool is_leap(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

int month_length(int year, int month) {
    static const int lengths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return (month == 2 && is_leap(year)) ? 29 : lengths[month - 1];
}

// The calendar a day at a time, from 0001-01-01, a Monday.
struct Day {
    int year, month, day, weekday;
    int64_t since_epoch;
    Day() : year(1), month(1), day(1), weekday(1), since_epoch(-719162) {}
    void next() {
        if (++day > month_length(year, month)) {
            day = 1;
            if (++month > 12) {
                month = 1;
                ++year;
            }
        }
        weekday = (weekday + 1) % 7;
        ++since_epoch;
    }
};

std::string written(const ebisu_civil_time_t& c) {
    char s[64];
    snprintf(s, sizeof(s), "%04d-%02d-%02dT%02d:%02d:%02d weekday %d", c.year, c.month,
            c.day, c.hour, c.minute, c.second, c.weekday);
    return s;
}

} // namespace

TEST_CASE( "ebisu_time_from_civil() converts UTC dates and times, before 1970 too" ) {
    int64_t s = 0;
    CHECK( ebisu_time_from_civil(1970, 1, 1, 0, 0, 0, &s) );
    CHECK( s == 0 );
    CHECK( ebisu_time_from_civil(1994, 11, 6, 8, 49, 37, &s) );
    CHECK( s == 784111777 );
    CHECK( ebisu_time_from_civil(1969, 12, 31, 23, 59, 59, &s) );
    CHECK( s == -1 );
    CHECK( ebisu_time_from_civil(1900, 3, 1, 0, 0, 0, &s) );
    CHECK( s == -2203891200LL );
    CHECK( ebisu_time_from_civil(1600, 2, 29, 12, 0, 0, &s) );
    CHECK( s == -11670955200LL );
    CHECK( ebisu_time_from_civil(1, 1, 1, 0, 0, 0, &s) );
    CHECK( s == -62135596800LL );
    CHECK( ebisu_time_from_civil(9999, 12, 31, 23, 59, 59, &s) );
    CHECK( s == 253402300799LL );
    CHECK( ebisu_time_from_civil(2016, 12, 31, 23, 59, 60, &s) );
    CHECK( s == 1483228800 );

    s = 12345;
    const int refused[][6] = {
        { 0, 1, 1, 0, 0, 0 }, { 10000, 1, 1, 0, 0, 0 },
        { 2024, 0, 1, 0, 0, 0 }, { 2024, 13, 1, 0, 0, 0 },
        { 2024, 1, 0, 0, 0, 0 }, { 2024, 4, 31, 0, 0, 0 },
        { 2025, 2, 29, 0, 0, 0 }, { 1900, 2, 29, 0, 0, 0 },
        { 2024, 1, 1, 24, 0, 0 }, { 2024, 1, 1, -1, 0, 0 },
        { 2024, 1, 1, 0, 60, 0 }, { 2024, 1, 1, 0, 0, 61 },
        { 2024, 1, 1, 12, 0, 60 }, { 2024, 1, 1, 0, 0, -1 },
    };
    for (const auto& f : refused) {
        INFO( f[0] << "-" << f[1] << "-" << f[2] << " " << f[3] << ":" << f[4] << ":" << f[5] );
        CHECK_FALSE( ebisu_time_from_civil(f[0], f[1], f[2], f[3], f[4], f[5], &s) );
    }
    // Refused, it leaves the output alone.
    CHECK( s == 12345 );
    CHECK_FALSE( ebisu_time_from_civil(2024, 1, 1, 0, 0, 0, NULL) );
}

TEST_CASE( "ebisu_time_to_civil() converts seconds since the epoch, before 1970 too" ) {
    ebisu_civil_time_t c;
    REQUIRE( ebisu_time_to_civil(0, &c) );
    CHECK( written(c) == "1970-01-01T00:00:00 weekday 4" );
    REQUIRE( ebisu_time_to_civil(784111777, &c) );
    CHECK( written(c) == "1994-11-06T08:49:37 weekday 0" );
    REQUIRE( ebisu_time_to_civil(-1, &c) );
    CHECK( written(c) == "1969-12-31T23:59:59 weekday 3" );
    REQUIRE( ebisu_time_to_civil(-86400, &c) );
    CHECK( written(c) == "1969-12-31T00:00:00 weekday 3" );
    REQUIRE( ebisu_time_to_civil(-86401, &c) );
    CHECK( written(c) == "1969-12-30T23:59:59 weekday 2" );
    REQUIRE( ebisu_time_to_civil(951782400, &c) );
    CHECK( written(c) == "2000-02-29T00:00:00 weekday 2" );
    REQUIRE( ebisu_time_to_civil(-11670955200LL, &c) );
    CHECK( written(c) == "1600-02-29T12:00:00 weekday 2" );
    REQUIRE( ebisu_time_to_civil(-62135596800LL, &c) );
    CHECK( written(c) == "0001-01-01T00:00:00 weekday 1" );
    REQUIRE( ebisu_time_to_civil(253402300799LL, &c) );
    CHECK( written(c) == "9999-12-31T23:59:59 weekday 5" );
    // A leap second comes back as the next day's midnight.
    REQUIRE( ebisu_time_to_civil(1483228800, &c) );
    CHECK( written(c) == "2017-01-01T00:00:00 weekday 0" );
    // A 32-bit time_t's last second, and the one after it.
    REQUIRE( ebisu_time_to_civil(INT32_MAX, &c) );
    CHECK( written(c) == "2038-01-19T03:14:07 weekday 2" );
    REQUIRE( ebisu_time_to_civil((int64_t)INT32_MAX + 1, &c) );
    CHECK( written(c) == "2038-01-19T03:14:08 weekday 2" );

    // Out of range, it leaves the output alone.
    c.year = 12345;
    CHECK_FALSE( ebisu_time_to_civil(-62135596801LL, &c) );
    CHECK_FALSE( ebisu_time_to_civil(253402300800LL, &c) );
    CHECK_FALSE( ebisu_time_to_civil(INT64_MIN, &c) );
    CHECK_FALSE( ebisu_time_to_civil(INT64_MAX, &c) );
    CHECK( c.year == 12345 );
    CHECK_FALSE( ebisu_time_to_civil(0, NULL) );
}

// Every day from 0001-01-01 to 9999-12-31, against a calendar counted a day at
// a time: to the date and back, with a time of day that changes from day to
// day so that every hour, minute and second comes up, and the day's first and
// last seconds.
TEST_CASE( "ebisu_time_to_civil() and ebisu_time_from_civil() agree on every day" ) {
    int64_t days = 0;
    int64_t wrong = 0;
    std::string first_wrong;
    for (Day d; d.year <= 9999; d.next(), ++days) {
        const int64_t midnight = d.since_epoch * 86400;
        const int64_t seconds[] = {
            midnight,
            midnight + (d.since_epoch * 7 % 24 + 24) % 24 * 3600 +
                (d.since_epoch * 13 % 60 + 60) % 60 * 60 + (d.since_epoch * 17 % 60 + 60) % 60,
            midnight + 86399,
        };
        for (int64_t s : seconds) {
            ebisu_civil_time_t c;
            int64_t back = 0;
            const int64_t of_day = s - midnight;
            if (!ebisu_time_to_civil(s, &c) || c.year != d.year || c.month != d.month ||
                    c.day != d.day || c.weekday != d.weekday || c.hour != of_day / 3600 ||
                    c.minute != of_day / 60 % 60 || c.second != of_day % 60 ||
                    !ebisu_time_from_civil(c.year, c.month, c.day, c.hour, c.minute, c.second,
                        &back) || back != s) {
                if (wrong++ == 0) {
                    first_wrong = std::to_string(s) + " gave " + written(c);
                }
            }
        }
    }
    INFO( first_wrong );
    CHECK( wrong == 0 );
    // 9999 years of 365.2425 days.
    CHECK( days == 3652059 );
}

TEST_CASE( "ebisu_time_is_leap_year() and ebisu_time_days_in_month() as the Gregorian calendar has them" ) {
    CHECK( ebisu_time_is_leap_year(2024) );
    CHECK( ebisu_time_is_leap_year(2000) );
    CHECK( ebisu_time_is_leap_year(1600) );
    CHECK_FALSE( ebisu_time_is_leap_year(2023) );
    CHECK_FALSE( ebisu_time_is_leap_year(1900) );
    CHECK_FALSE( ebisu_time_is_leap_year(2100) );
    // Outside the conversions' range too.
    CHECK( ebisu_time_is_leap_year(0) );
    CHECK( ebisu_time_is_leap_year(-4) );
    CHECK_FALSE( ebisu_time_is_leap_year(-100) );
    CHECK( ebisu_time_is_leap_year(10000) );

    const int lengths[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for (int month = 1; month <= 12; ++month) {
        INFO( month );
        CHECK( ebisu_time_days_in_month(2023, month) == lengths[month - 1] );
        CHECK( ebisu_time_days_in_month(2024, month) == (month == 2 ? 29 : lengths[month - 1]) );
    }
    CHECK( ebisu_time_days_in_month(1900, 2) == 28 );
    CHECK( ebisu_time_days_in_month(2000, 2) == 29 );
    CHECK( ebisu_time_days_in_month(2024, 0) == 0 );
    CHECK( ebisu_time_days_in_month(2024, 13) == 0 );
    CHECK( ebisu_time_days_in_month(2024, -1) == 0 );

    // Every year of the conversions, as they take a date: its last day of
    // each month, and not the day after.
    int64_t wrong = 0;
    for (int year = 1; year <= 9999; ++year) {
        if (ebisu_time_is_leap_year(year) != is_leap(year)) {
            ++wrong;
        }
        for (int month = 1; month <= 12; ++month) {
            int64_t s = 0;
            const int days = ebisu_time_days_in_month(year, month);
            if (days != month_length(year, month) ||
                    !ebisu_time_from_civil(year, month, days, 0, 0, 0, &s) ||
                    ebisu_time_from_civil(year, month, days + 1, 0, 0, 0, &s)) {
                ++wrong;
            }
        }
    }
    CHECK( wrong == 0 );
}

namespace {

std::string iso8601(int64_t seconds, char sep, size_t size = EBISU_TIME_ISO8601_SIZE) {
    std::string buff(size, '#');
    bool ok = ebisu_time_format_iso8601(seconds, sep, size > 0 ? &buff[0] : nullptr, size);
    std::string s = size > 0 ? std::string(buff.c_str()) : std::string();
    return ok ? s : "false:" + s;
}

} // namespace

TEST_CASE( "ebisu_time_format_iso8601() writes a UTC date and time as RFC 3339 has it" ) {
    CHECK( iso8601(1793973201, 'T') == "2026-11-06T13:53:21Z" );
    CHECK( iso8601(1793973201, ' ') == "2026-11-06 13:53:21Z" );
    CHECK( iso8601(0, 'T') == "1970-01-01T00:00:00Z" );
    CHECK( iso8601(-1, 'T') == "1969-12-31T23:59:59Z" );
    CHECK( iso8601(1709251199, 'T') == "2024-02-29T23:59:59Z" );
    CHECK( iso8601(-62135596800LL, 'T') == "0001-01-01T00:00:00Z" );
    CHECK( iso8601(253402300799LL, 'T') == "9999-12-31T23:59:59Z" );
    CHECK( strlen("9999-12-31T23:59:59Z") + 1 == EBISU_TIME_ISO8601_SIZE );
    // A larger buffer, terminated after the 20 characters.
    CHECK( iso8601(0, 'T', 64) == "1970-01-01T00:00:00Z" );
}

TEST_CASE( "ebisu_time_format_iso8601() writes nothing it can't write whole" ) {
    CHECK( iso8601(-62135596801LL, 'T') == "false:" );
    CHECK( iso8601(253402300800LL, 'T') == "false:" );
    CHECK( iso8601(0, 'T', EBISU_TIME_ISO8601_SIZE - 1) == "false:" );
    CHECK( iso8601(0, 'T', 1) == "false:" );
    CHECK( iso8601(0, 'T', 0) == "false:" );
    CHECK( iso8601(0, '\0') == "false:" );
}

TEST_CASE( "ebisu_time_format_iso8601() agrees with ebisu_time_to_civil() on every day" ) {
    int64_t wrong = 0;
    std::string first_wrong;
    for (Day d; d.year <= 9999; d.next()) {
        const int64_t s = d.since_epoch * 86400 +
            (d.since_epoch * 7 % 24 + 24) % 24 * 3600 +
            (d.since_epoch * 13 % 60 + 60) % 60 * 60 + (d.since_epoch * 17 % 60 + 60) % 60;
        ebisu_civil_time_t c;
        char expected[64];
        if (!ebisu_time_to_civil(s, &c)) {
            ++wrong;
            continue;
        }
        snprintf(expected, sizeof(expected), "%04d-%02d-%02dT%02d:%02d:%02dZ", c.year,
                c.month, c.day, c.hour, c.minute, c.second);
        if (iso8601(s, 'T') != expected && wrong++ == 0) {
            first_wrong = expected;
        }
    }
    INFO( first_wrong );
    CHECK( wrong == 0 );
}
