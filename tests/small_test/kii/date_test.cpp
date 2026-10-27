#include "catch.hpp"
#include "kii.h"
#include "kii_impl.h"
#include "ebisu_version.h"
#include "test_callbacks.h"

#include <functional>
#include <sstream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace {

const char* const DAY_NAMES[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char* const DAY_NAMES_LONG[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
const char* const MONTH_NAMES[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// 2026-01-01T00:00:00Z
const int64_t Y2026 = 1767225600;

int64_t parse(const std::string& header, int64_t floor = 0) {
    return _parse_date(header.data(), header.size(), floor);
}

// The checks below compare the parser with a calendar of their own, which walks
// day by day rather than computing.
bool leap(int year) {
    if (year % 400 == 0) { return true; }
    if (year % 100 == 0) { return false; }
    return year % 4 == 0;
}

int month_length(int year, int month) {
    switch (month) {
    case 2: return leap(year) ? 29 : 28;
    case 4: case 6: case 9: case 11: return 30;
    default: return 31;
    }
}

struct Day {
    int year, month, day, weekday;
    int64_t since_epoch;
    Day() : year(1970), month(1), day(1), weekday(4), since_epoch(0) {}
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

// How a date is written: the three forms of RFC 9110, and IMF-fixdate with UTC.
enum Form { IMF, IMF_UTC, ASCTIME, ASCTIME_ZERO, RFC850 };

std::string written(Form form, const Day& d, int weekday, int hour, int minute, int second) {
    char s[64];
    const char* month = MONTH_NAMES[d.month - 1];
    switch (form) {
    case IMF:
    case IMF_UTC:
        snprintf(s, sizeof(s), "%s, %02d %s %04d %02d:%02d:%02d %s", DAY_NAMES[weekday],
                d.day, month, d.year, hour, minute, second, (form == IMF) ? "GMT" : "UTC");
        break;
    case ASCTIME:
    case ASCTIME_ZERO:
        snprintf(s, sizeof(s), (form == ASCTIME) ? "%s %s %2d %02d:%02d:%02d %04d" : "%s %s %02d %02d:%02d:%02d %04d",
                DAY_NAMES[weekday], month, d.day, hour, minute, second, d.year);
        break;
    case RFC850:
        snprintf(s, sizeof(s), "%s, %02d-%s-%02d %02d:%02d:%02d GMT", DAY_NAMES_LONG[weekday],
                d.day, month, d.year % 100, hour, minute, second);
        break;
    }
    return s;
}

// How seconds since the epoch are written, counting from 1970 a year and then a
// month at a time.
std::string written(Form form, int64_t seconds) {
    Day d;
    d.since_epoch = seconds / 86400;
    d.weekday = (int)((d.since_epoch + 4) % 7);
    int64_t days = d.since_epoch;
    while (days >= (leap(d.year) ? 366 : 365)) {
        days -= leap(d.year) ? 366 : 365;
        ++d.year;
    }
    while (days >= month_length(d.year, d.month)) {
        days -= month_length(d.year, d.month);
        ++d.month;
    }
    d.day = (int)days + 1;
    int64_t rest = seconds % 86400;
    return written(form, d, d.weekday, (int)(rest / 3600), (int)(rest / 60 % 60), (int)(rest % 60));
}

} // namespace

TEST_CASE( "Date headers in IMF-fixdate form are read as seconds since the epoch" ) {
    // RFC 9110's own example.
    CHECK( parse("Date: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date: Thu, 01 Jan 1970 00:00:00 GMT") == 0 );
    // Leap days, a century that is a leap year and one that is not.
    CHECK( parse("Date: Thu, 29 Feb 2024 12:00:00 GMT") == 1709208000 );
    CHECK( parse("Date: Tue, 29 Feb 2000 00:00:00 GMT") == 951782400 );
    CHECK( parse("Date: Wed, 31 Dec 2025 23:59:59 GMT") == 1767225599 );
    // Past 2^31 and 2^32 seconds, and the last date four digits hold.
    CHECK( parse("Date: Tue, 19 Jan 2038 03:14:08 GMT") == 2147483648LL );
    CHECK( parse("Date: Sun, 07 Feb 2106 06:28:16 GMT") == 4294967296LL );
    CHECK( parse("Date: Fri, 31 Dec 9999 23:59:59 GMT") == 253402300799LL );
    // A leap second, which ends a day.
    CHECK( parse("Date: Sat, 31 Dec 2016 23:59:60 GMT") == 1483228800 );
    CHECK( parse("Date: Tue, 30 Jun 2015 23:59:60 GMT") == 1435708800 );
    // UTC for GMT, which some servers send.
    CHECK( parse("Date: Sun, 06 Nov 1994 08:49:37 UTC") == 784111777 );

    // The name in any case, whitespace around the value or none.
    CHECK( parse("date: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("DATE: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("dAtE: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date:Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date: \t Sun, 06 Nov 1994 08:49:37 GMT \t ") == 784111777 );
}

TEST_CASE( "Date headers in asctime() form are read, always as GMT" ) {
    // RFC 9110's own example, the day padded with a space, or with a zero.
    CHECK( parse("Date: Sun Nov  6 08:49:37 1994") == 784111777 );
    CHECK( parse("Date: Sun Nov 06 08:49:37 1994") == 784111777 );
    CHECK( parse("Date: Thu Feb 29 12:00:00 2024") == 1709208000 );
    CHECK( parse("Date: Thu Jan  1 00:00:00 1970") == 0 );
    CHECK( parse("Date: Sat Dec 31 23:59:60 2016") == 1483228800 );
    CHECK( parse("Date:Sun Nov  6 08:49:37 1994 ") == 784111777 );

    const char* malformed[] = {
        // One space only before a single digit, or none.
        "Date: Sun Nov 6 08:49:37 1994",
        "Date: Sun Nov6 08:49:37 1994",
        // A zone, which the form has none of.
        "Date: Sun Nov  6 08:49:37 1994 GMT",
        "Date: Sun Nov  6 08:49:37 GMT 1994",
        // The day's name has to agree, and be the short one.
        "Date: Mon Nov  6 08:49:37 1994",
        "Date: Sunday Nov  6 08:49:37 1994",
        // Out of range, or not digits.
        "Date: Thu Jan  0 00:00:00 1970",
        "Date: Sun Nov  x 08:49:37 1994",
        "Date: Sun Nov  6 08:49:37 94",
        "Date: Wed Dec 31 23:59:59 1969",
    };
    for (const char* header : malformed) {
        INFO( header );
        CHECK( parse(header) == -2 );
    }
}

TEST_CASE( "Date headers in RFC 850 form are read, their year as RFC 9110 asks" ) {
    // With a floor in 1970, the present is 1970: two digits up to 20 are a year
    // up to 2020, and 21 to 69 more than 50 years ahead, so 1921 to 1969.
    CHECK( parse("Date: Sunday, 06-Nov-94 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date: Thursday, 01-Jan-70 00:00:00 GMT") == 0 );
    CHECK( parse("Date: Saturday, 01-Jan-77 00:00:00 GMT") == 220924800 );
    CHECK( parse("Date: Monday, 15-Jun-20 12:00:00 GMT") == 1592222400 );
    CHECK( parse("Date: Wednesday, 15-Jun-21 12:00:00 GMT") == -2 );
    // With a floor in 2026: 26 to 76 are 2026 to 2076, 77 to 99 1977 to 1999,
    // which the floor then refuses.
    CHECK( parse("Date: Tuesday, 27-Oct-26 00:00:00 GMT", Y2026) == 1793059200 );
    CHECK( parse("Date: Thursday, 31-Dec-76 23:59:59 GMT", Y2026) == 3376684799LL );
    CHECK( parse("Date: Saturday, 01-Jan-77 00:00:00 GMT", Y2026) == -2 );
    CHECK( parse("Date: Sunday, 06-Nov-94 08:49:37 GMT", Y2026) == -2 );
    // Even where the day's name fits the year ahead: 2080-01-01 was a Monday,
    // 1980-01-01 a Tuesday.
    CHECK( parse("Date: Monday, 01-Jan-80 00:00:00 GMT", Y2026) == -2 );
    // UTC for GMT, and whitespace around the value.
    CHECK( parse("Date: Sunday, 06-Nov-94 08:49:37 UTC") == 784111777 );
    CHECK( parse("Date:\tSunday, 06-Nov-94 08:49:37 GMT\t") == 784111777 );

    const char* malformed[] = {
        // The day's whole name, agreeing with the date.
        "Date: Sun, 06-Nov-94 08:49:37 GMT",
        "Date: Monday, 06-Nov-94 08:49:37 GMT",
        "Date: sunday, 06-Nov-94 08:49:37 GMT",
        "Date: Sunday 06-Nov-94 08:49:37 GMT",
        "Date: Sunday,06-Nov-94 08:49:37 GMT",
        // Two digits of the year, separated by dashes.
        "Date: Sunday, 06-Nov-1994 08:49:37 GMT",
        "Date: Sunday, 06 Nov 94 08:49:37 GMT",
        "Date: Sunday, 06-Nov-9x 08:49:37 GMT",
        // A zone, GMT or UTC.
        "Date: Sunday, 06-Nov-94 08:49:37",
        "Date: Sunday, 06-Nov-94 08:49:37 PST",
    };
    for (const char* header : malformed) {
        INFO( header );
        CHECK( parse(header) == -2 );
    }
}

TEST_CASE( "A date earlier than the floor is not taken" ) {
    CHECK( parse("Date: Thu, 01 Jan 2026 00:00:00 GMT", Y2026) == Y2026 );
    CHECK( parse("Date: Wed, 31 Dec 2025 23:59:59 GMT", Y2026) == -2 );
    CHECK( parse("Date: Wed Dec 31 23:59:59 2025", Y2026) == -2 );
    CHECK( parse("Date: Thu, 01 Jan 1970 00:00:00 GMT", Y2026) == -2 );
    // A floor before the epoch, or at it, refuses nothing.
    CHECK( parse("Date: Thu, 01 Jan 1970 00:00:00 GMT", -1) == 0 );
    CHECK( parse("Date: Thu, 01 Jan 1970 00:00:00 GMT", 0) == 0 );
}

TEST_CASE( "Other headers are not Date headers" ) {
    const char* others[] = {
        "",
        "D",
        "Date",
        "HTTP/1.1 200 OK",
        "Content-Length: 29",
        "X-Date: Sun, 06 Nov 1994 08:49:37 GMT",
        "Dates: Sun, 06 Nov 1994 08:49:37 GMT",
        // No whitespace allowed before the colon, or before the name.
        "Date : Sun, 06 Nov 1994 08:49:37 GMT",
        " Date: Sun, 06 Nov 1994 08:49:37 GMT",
    };
    for (const char* header : others) {
        INFO( header );
        CHECK( parse(header) == -1 );
    }
    CHECK( _parse_date(NULL, 0, 0) == -1 );
    CHECK( _parse_date(NULL, 35, 0) == -1 );
}

TEST_CASE( "A Date header in another form has no date" ) {
    const char* malformed[] = {
        "Date:",
        "Date: ",
        // Truncated, or followed by something else.
        "Date: Sun, 06 Nov 1994 08:49:37 GM",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT+1",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT x",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT\r",
        // Neither GMT nor UTC, nor either in another case: no offsets, local zones
        // or ISO 8601.
        "Date: Sun, 06 Nov 1994 08:49:37 gmt",
        "Date: Sun, 06 Nov 1994 08:49:37 utc",
        "Date: Sun, 06 Nov 1994 08:49:37 +0000",
        "Date: Sun, 06 Nov 1994 08:49:37 -0000",
        "Date: Sun, 06 Nov 1994 17:49:37 +0900",
        "Date: Sun, 06 Nov 1994 17:49:37 JST",
        "Date: Sun, 06 Nov 1994 08:49:37 Z",
        "Date: 1994-11-06T08:49:37Z",
        // Names are case-sensitive, and the day's has to match the date.
        "Date: sun, 06 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 nov 1994 08:49:37 GMT",
        "Date: Mon, 06 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Foo 1994 08:49:37 GMT",
        // Day and month names in another language.
        "Date: Dim, 06 nov 1994 08:49:37 GMT",
        // Out of range.
        "Date: Mon, 00 Nov 1994 08:49:37 GMT",
        "Date: Thu, 31 Nov 1994 08:49:37 GMT",
        "Date: Sat, 29 Feb 2025 00:00:00 GMT",
        "Date: Mon, 29 Feb 2100 00:00:00 GMT",
        "Date: Sun, 06 Nov 1994 24:00:00 GMT",
        "Date: Sun, 06 Nov 1994 08:60:00 GMT",
        "Date: Sun, 06 Nov 1994 08:49:61 GMT",
        // A leap second anywhere but at the end of a day.
        "Date: Sun, 06 Nov 1994 08:49:60 GMT",
        "Date: Sat, 31 Dec 2016 23:58:60 GMT",
        "Date: Sat, 31 Dec 2016 22:59:60 GMT",
        // Before the epoch: a 32-bit time_t wrapped past 2038 lands in 1901.
        "Date: Wed, 31 Dec 1969 23:59:59 GMT",
        "Date: Fri, 13 Dec 1901 20:45:52 GMT",
        "Date: Mon, 01 Jan 0000 00:00:00 GMT",
        // Not digits, or not where they should be.
        "Date: Sun, 0a Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Nov 19x4 08:49:37 GMT",
        "Date: Sun, -6 Nov 1994 08:49:37 GMT",
        "Date: Sun, +6 Nov 1994 08:49:37 GMT",
        "Date: Sun,  6 Nov 1994 08:49:37 GMT",
        "Date: Sun, 6 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Nov 94 08:49:37 GMT",
        "Date: Sun, 06 Nov 1994 08.49.37 GMT",
        "Date: Sun 06 Nov 1994 08:49:37 GMT ",
        "Date: Sun, 06 Nov 1994  8:49:37 GMT",
    };
    for (const char* header : malformed) {
        INFO( header );
        CHECK( parse(header) == -2 );
    }
}

TEST_CASE( "Every day from 1970 to 2400 is read as the calendar counts it" ) {
    // A whole 400-year cycle and then some: 2000 and 2400 are leap years, 2100,
    // 2200 and 2300 are not. The time of day changes from day to day so that
    // every hour, minute and second comes up. Every day is checked in
    // IMF-fixdate, and in one of the other forms too, by turns, with its own
    // day's name and with another. RFC 850's years are read against a floor in
    // 1970 up to 2020, and against one in 2026 from then to 2076.
    int64_t days = 0;
    int64_t wrong = 0;
    std::string first_wrong;
    const Form others[] = { IMF_UTC, ASCTIME, ASCTIME_ZERO, RFC850 };
    for (Day d; d.year <= 2400; d.next(), ++days) {
        int hour = (int)(d.since_epoch * 7 % 24);
        int minute = (int)(d.since_epoch * 13 % 60);
        int second = (int)(d.since_epoch * 17 % 60);
        const int64_t expected = d.since_epoch * 86400 + hour * 3600 + minute * 60 + second;
        int other_day = (d.weekday + 1 + (int)(d.since_epoch % 6)) % 7;
        Form other = others[d.since_epoch % 4];
        int64_t floor = 0;
        if (other == RFC850) {
            if (d.year >= 2026 && d.year <= 2076) {
                floor = Y2026;
            } else if (d.year > 2020) {
                other = IMF_UTC;
            }
        }
        std::string failed;
        if (parse("Date: " + written(IMF, d, d.weekday, hour, minute, second)) != expected) {
            failed = written(IMF, d, d.weekday, hour, minute, second);
        } else if (parse("Date: " + written(IMF, d, other_day, hour, minute, second)) != -2) {
            failed = written(IMF, d, other_day, hour, minute, second);
        } else if (parse("Date: " + written(other, d, d.weekday, hour, minute, second), floor) != expected) {
            failed = written(other, d, d.weekday, hour, minute, second);
        } else if (parse("Date: " + written(other, d, other_day, hour, minute, second), floor) != -2) {
            failed = written(other, d, other_day, hour, minute, second);
        }
        if (!failed.empty() && wrong++ == 0) {
            first_wrong = failed;
        }
    }
    INFO( "first wrong: " << first_wrong );
    CHECK( wrong == 0 );
    CHECK( days == 157420 );
}

TEST_CASE( "A Date changed by one byte is taken only if it is still a date, written as such" ) {
    const struct { Form form; const char* value; } dates[] = {
        { IMF, "Sun, 06 Nov 1994 08:49:37 GMT" },
        { IMF, "Thu, 29 Feb 2024 12:00:00 GMT" },
        { IMF, "Fri, 31 Dec 9999 23:59:59 GMT" },
        { IMF, "Thu, 01 Jan 1970 00:00:00 GMT" },
        { IMF_UTC, "Sun, 06 Nov 1994 08:49:37 UTC" },
        { ASCTIME, "Sun Nov  6 08:49:37 1994" },
        { ASCTIME_ZERO, "Thu Feb 29 12:00:00 2024" },
        { RFC850, "Sunday, 06-Nov-94 08:49:37 GMT" },
        { RFC850, "Wednesday, 31-Dec-69 23:59:59 GMT" },
    };
    int64_t taken = 0;
    for (const auto& date : dates) {
        const std::string value(date.value);
        for (size_t i = 0; i < value.size(); ++i) {
            for (int b = 0; b < 256; ++b) {
                std::string changed = value;
                changed[i] = (char)b;
                int64_t seconds = parse("Date: " + changed);
                if (seconds >= 0) {
                    // Whatever is taken is exactly how that date is written, in
                    // the same form; asctime()'s day either way.
                    INFO( "taken: " << changed );
                    bool same = written(date.form, seconds) == changed ||
                        (date.form == ASCTIME && written(ASCTIME_ZERO, seconds) == changed) ||
                        (date.form == ASCTIME_ZERO && written(ASCTIME, seconds) == changed);
                    REQUIRE( same );
                    ++taken;
                } else {
                    INFO( "refused: " << changed );
                    REQUIRE( seconds == -2 );
                }
            }
        }
    }
    // Some changes are other valid dates: another digit of the year, say.
    CHECK( taken > 0 );
}

TEST_CASE( "A Date cut short or followed by anything but whitespace has no date" ) {
    const std::string headers[] = {
        "Date: Sun, 06 Nov 1994 08:49:37 GMT",
        "Date: Sun Nov  6 08:49:37 1994",
        "Date: Sunday, 06-Nov-94 08:49:37 GMT",
    };
    for (const std::string& header : headers) {
        for (size_t n = 0; n < header.size(); ++n) {
            INFO( header.substr(0, n) );
            CHECK( parse(header.substr(0, n)) == ((n < 5) ? -1 : -2) );
        }
        for (int b = 0; b < 256; ++b) {
            INFO( header << " followed by " << b );
            bool whitespace = (b == ' ' || b == '\t');
            CHECK( parse(header + (char)b) == (whitespace ? 784111777 : -2) );
        }
    }
}

namespace {

// A kii_t whose requests are each answered, on a connection of their own, by
// the next of responses.
struct Client {
    char buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource;
    kii_t kii;
    std::vector<std::string> responses;
    size_t next;
    std::string resp;
    size_t resp_pos;
    khct::cb::SockCtx http_ctx;

    // Most of these responses are dated 1994: unless asked, refuse nothing.
    explicit Client(bool keep_floor = false) : next(0), resp_pos(0) {
        resource.tokens = tokens;
        resource.tokens_num = 64;
        kii_init(&kii);
        if (!keep_floor) {
            kii._resp_date_floor = 0;
        }
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "dummyAppID");
        kii_set_buff(&kii, buff, sizeof(buff));
        kii_set_json_parser_resource(&kii, &resource);
        http_ctx.on_connect = [this](void*, const char*, unsigned int) {
            resp = (next < responses.size()) ? responses[next] : std::string();
            ++next;
            resp_pos = 0;
            return KHC_SOCK_OK;
        };
        http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        http_ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            size_t n = resp.size() - resp_pos;
            if (n > length_to_read) { n = length_to_read; }
            memcpy(buffer, resp.data() + resp_pos, n);
            resp_pos += n;
            *out_actual_length = n;
            return KHC_SOCK_OK;
        };
        http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
        kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
        kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
        kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
        kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    }

    kii_code_t get() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, KII_FALSE);
        return (res == KII_ERR_OK) ? kii_api_call_run(&kii) : res;
    }
};

std::string response(const std::string& status_line, const std::vector<std::string>& headers) {
    std::string r = status_line + "\r\n";
    for (const std::string& h : headers) {
        r += h + "\r\n";
    }
    return r + "Content-Length: 2\r\n\r\n{}";
}

const std::string NOV_1994 = "Date: Sun, 06 Nov 1994 08:49:37 GMT";
const std::string FEB_2024 = "Date: Thu, 29 Feb 2024 12:00:00 GMT";

} // namespace

TEST_CASE( "kii_get_resp_date() gives the Date of the last request's response" ) {
    Client c;
    // None before any request.
    REQUIRE( kii_get_resp_date(&c.kii) == -1 );

    c.responses = {
        response("HTTP/1.1 200 OK", { NOV_1994 }),
        // An error response states the time too.
        response("HTTP/1.1 404 Not Found", { FEB_2024 }),
        // A response without one leaves no date from the earlier ones.
        response("HTTP/1.1 200 OK", {}),
        // The obsolete forms are dates too.
        response("HTTP/1.1 200 OK", { "Date: Sunday, 06-Nov-94 08:49:37 GMT" }),
        // A Date in none of the three forms is not.
        response("HTTP/1.1 200 OK", { "Date: 1994-11-06T08:49:37Z" }),
    };
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );

    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&c.kii) == 404 );
    CHECK( kii_get_resp_date(&c.kii) == 1709208000 );

    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );

    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );

    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "A response whose Dates are ambiguous has no date" ) {
    Client c;
    c.responses = {
        // The same Date twice is still that date.
        response("HTTP/1.1 200 OK", { NOV_1994, "date:  Sun, 06 Nov 1994 08:49:37 GMT" }),
        // Two that disagree, in either order.
        response("HTTP/1.1 200 OK", { NOV_1994, FEB_2024 }),
        response("HTTP/1.1 200 OK", { FEB_2024, NOV_1994 }),
        // A valid one beside a malformed one, in either order.
        response("HTTP/1.1 200 OK", { NOV_1994, "Date: tomorrow" }),
        response("HTTP/1.1 200 OK", { "Date: tomorrow", NOV_1994 }),
        // Three, two of which agree.
        response("HTTP/1.1 200 OK", { NOV_1994, FEB_2024, NOV_1994 }),
    };
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );
    for (int i = 0; i < 5; ++i) {
        INFO( c.responses[c.next] );
        REQUIRE( c.get() == KII_ERR_OK );
        CHECK( kii_get_resp_date(&c.kii) == -1 );
    }
    // And the next response is read afresh.
    c.responses.push_back(response("HTTP/1.1 200 OK", { FEB_2024 }));
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == 1709208000 );
}

TEST_CASE( "A call that sends nothing leaves no date from an earlier one" ) {
    Client c;
    c.responses = { response("HTTP/1.1 200 OK", { NOV_1994 }) };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == 784111777 );

    // The call starts, and fails before any request goes out.
    REQUIRE( kii_api_call_start(&c.kii, "POST", "/api/apps/dummyAppID/x",
                "application/json", KII_FALSE) == KII_ERR_OK );
    const std::string body(2 * sizeof(c.buff), 'x');
    REQUIRE( kii_api_call_append_body(&c.kii, body.data(), body.size()) == KII_ERR_TOO_LARGE_DATA );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
    CHECK( c.next == 1 );
}

TEST_CASE( "A Date after a header too long for the buffer is still read" ) {
    Client c;
    static char resp_header_buff[64];
    kii_set_resp_header_buff(&c.kii, resp_header_buff, sizeof(resp_header_buff));
    c.responses = {
        // khc skips a header longer than its response header buffer.
        response("HTTP/1.1 200 OK", { "Set-Cookie: c=" + std::string(200, 'x'), NOV_1994 }),
        // Including a Date padded past it, which then isn't one.
        response("HTTP/1.1 200 OK", { NOV_1994 + std::string(64, ' ') }),
    };
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "A Date among a chunked response's trailers is not taken" ) {
    Client c;
    c.responses = {
        "HTTP/1.1 200 OK\r\n" + NOV_1994 + "\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "2\r\n{}\r\n"
        "0\r\n" + FEB_2024 + "\r\n"
        "\r\n",
        "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "2\r\n{}\r\n"
        "0\r\n" + FEB_2024 + "\r\n"
        "\r\n",
    };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_body_length(&c.kii) == 2 );
    CHECK( kii_get_resp_date(&c.kii) == 784111777 );
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "A request that gets no response leaves no date from an earlier one" ) {
    Client c;
    // The second connection is answered with nothing at all.
    c.responses = { response("HTTP/1.1 200 OK", { NOV_1994 }), "" };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == 784111777 );
    REQUIRE( c.get() != KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "A request sent again without a new start, and not answered, has no date" ) {
    Client c;
    c.responses = { response("HTTP/1.1 200 OK", { NOV_1994 }), "" };
    REQUIRE( kii_api_call_start(&c.kii, "GET", "/api/apps/dummyAppID", NULL, KII_FALSE) == KII_ERR_OK );
    REQUIRE( kii_api_call_run(&c.kii) == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == 784111777 );
    // Neither a call starting nor a response clears it here: sending does.
    REQUIRE( kii_api_call_run(&c.kii) != KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}

TEST_CASE( "kii_init() sets a floor shortly before the source was built" ) {
    Client c(true);
    REQUIRE( c.kii._resp_date_floor <= EBISU_SOURCE_TIME );
    REQUIRE( c.kii._resp_date_floor > EBISU_SOURCE_TIME - 30 * 86400 );
    const std::string after = "Date: " + written(IMF, EBISU_SOURCE_TIME + 86400);
    const std::string before = "Date: " + written(IMF, EBISU_SOURCE_TIME - 30 * 86400);
    c.responses = {
        response("HTTP/1.1 200 OK", { after }),
        // A server with no clock set, a month before the source, or 1994.
        response("HTTP/1.1 200 OK", { "Date: Thu, 01 Jan 1970 00:00:00 GMT" }),
        response("HTTP/1.1 200 OK", { before }),
        response("HTTP/1.1 200 OK", { NOV_1994 }),
    };
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == EBISU_SOURCE_TIME + 86400 );
    for (int i = 0; i < 3; ++i) {
        INFO( c.responses[c.next] );
        REQUIRE( c.get() == KII_ERR_OK );
        CHECK( kii_get_resp_date(&c.kii) == -1 );
    }
}
