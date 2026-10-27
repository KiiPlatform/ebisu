#include "catch.hpp"
#include "kii.h"
#include "test_callbacks.h"

#include <string>
#include <string.h>
#include <vector>

// khc reads the Date: its tests are in small_test_khc. These are what kii adds,
// a date per API call.

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
            kii._khc._resp_date_floor = 0;
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

TEST_CASE( "kii_init() refuses dates from before the source was built" ) {
    Client c(true);
    c.responses = {
        response("HTTP/1.1 200 OK", { "Date: Thu, 01 Jan 1970 00:00:00 GMT" }),
        response("HTTP/1.1 200 OK", { NOV_1994 }),
    };
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( kii_get_resp_date(&c.kii) == -1 );
}
