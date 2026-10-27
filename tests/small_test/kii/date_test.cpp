#include "catch.hpp"
#include "kii.h"
#include "kii_impl.h"
#include "test_callbacks.h"

#include <string>
#include <string.h>
#include <vector>

static int64_t parse(const std::string& header) {
    return _parse_date(header.data(), header.size());
}

TEST_CASE( "Date headers in IMF-fixdate form are read as seconds since the epoch" ) {
    // RFC 9110's own example.
    CHECK( parse("Date: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date: Thu, 01 Jan 1970 00:00:00 GMT") == 0 );
    // Leap days, a century that is a leap year and one that is not.
    CHECK( parse("Date: Thu, 29 Feb 2024 12:00:00 GMT") == 1709208000 );
    CHECK( parse("Date: Tue, 29 Feb 2000 00:00:00 GMT") == 951782400 );
    CHECK( parse("Date: Wed, 31 Dec 2025 23:59:59 GMT") == 1767225599 );
    // Past 2^31 and 2^32 seconds.
    CHECK( parse("Date: Tue, 19 Jan 2038 03:14:08 GMT") == 2147483648LL );
    CHECK( parse("Date: Sun, 07 Feb 2106 06:28:16 GMT") == 4294967296LL );
    // A leap second.
    CHECK( parse("Date: Sat, 31 Dec 2016 23:59:60 GMT") == 1483228800 );

    // The name in any case, whitespace around the value or none.
    CHECK( parse("date: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("DATE: Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date:Sun, 06 Nov 1994 08:49:37 GMT") == 784111777 );
    CHECK( parse("Date: \t Sun, 06 Nov 1994 08:49:37 GMT \t ") == 784111777 );
}

TEST_CASE( "Anything but a Date header in IMF-fixdate form is no date" ) {
    const char* not_dates[] = {
        "",
        "Date",
        "Date:",
        "Date: ",
        "Content-Length: 29",
        "X-Date: Sun, 06 Nov 1994 08:49:37 GMT",
        "Dates: Sun, 06 Nov 1994 08:49:37 GMT",
        // No whitespace allowed before the colon.
        "Date : Sun, 06 Nov 1994 08:49:37 GMT",
        // The obsolete forms: RFC 850 and asctime().
        "Date: Sunday, 06-Nov-94 08:49:37 GMT",
        "Date: Sun Nov  6 08:49:37 1994",
        // Truncated, or followed by something else.
        "Date: Sun, 06 Nov 1994 08:49:37 GM",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT+1",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT x",
        // Not GMT, or not in its case.
        "Date: Sun, 06 Nov 1994 08:49:37 UTC",
        "Date: Sun, 06 Nov 1994 08:49:37 gmt",
        // Names are case-sensitive, and the day's has to match the date.
        "Date: sun, 06 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 nov 1994 08:49:37 GMT",
        "Date: Mon, 06 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Foo 1994 08:49:37 GMT",
        // Out of range.
        "Date: Mon, 00 Nov 1994 08:49:37 GMT",
        "Date: Thu, 31 Nov 1994 08:49:37 GMT",
        "Date: Sat, 29 Feb 2025 00:00:00 GMT",
        "Date: Mon, 29 Feb 2100 00:00:00 GMT",
        "Date: Sun, 06 Nov 1994 24:00:00 GMT",
        "Date: Sun, 06 Nov 1994 08:60:00 GMT",
        "Date: Sun, 06 Nov 1994 08:49:61 GMT",
        "Date: Wed, 31 Dec 1969 23:59:59 GMT",
        // Not digits, or not where they should be.
        "Date: Sun, 0a Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Nov 19x4 08:49:37 GMT",
        "Date: Sun, -6 Nov 1994 08:49:37 GMT",
        "Date: Sun,  6 Nov 1994 08:49:37 GMT",
        "Date: Sun, 06 Nov 1994 08.49.37 GMT",
        "Date: Sun 06 Nov 1994 08:49:37 GMT ",
    };
    for (const char* header : not_dates) {
        INFO( header );
        CHECK( parse(header) == -1 );
    }
}

// Each request opens its own connection, answered with resp.
TEST_CASE( "kii_get_resp_date() gives the Date of the last request's response" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);

    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        resp_pos = 0;
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_recv = [&resp, &resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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

    auto send = [&kii]() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, KII_FALSE);
        return (res == KII_ERR_OK) ? kii_api_call_run(&kii) : res;
    };

    // None before any request.
    REQUIRE( kii_get_resp_date(&kii) == -1 );

    resp = "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n"
        "Date: Sun, 06 Nov 1994 08:49:37 GMT\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&kii) == 784111777 );

    // An error response states the time too.
    resp = "HTTP/1.1 404 Not Found\r\n"
        "Date: Thu, 29 Feb 2024 12:00:00 GMT\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    REQUIRE( kii_get_resp_status(&kii) == 404 );
    CHECK( kii_get_resp_date(&kii) == 1709208000 );

    // A response without one leaves no date from the earlier ones.
    resp = "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&kii) == -1 );

    // Nor does one whose Date isn't in IMF-fixdate form.
    resp = "HTTP/1.1 200 OK\r\n"
        "Date: Sunday, 06-Nov-94 08:49:37 GMT\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&kii) == -1 );
}
