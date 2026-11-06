#include "catch.hpp"
#include <khc.h>
#include "http_script.h"
#include <string>
#include <utility>
#include <vector>

// khc printed every request and response to stdout under KHC_TRACE, bodies,
// tokens and passwords included. It logs them to a logger now, without
// those.

namespace {

struct Fixture {
    khc http;
    khct::HttpScript script;
    khct::cb::IOCtx io;
    khct::cb::LoggerCtx log;
    khc_slist* headers = nullptr;
    std::string body;
    size_t body_read = 0;

    Fixture() {
        khc_init(&http);
        khc_set_host(&http, "api.kii.com");
        khc_set_method(&http, "POST");
        khc_set_path(&http, "/api/apps/app1/onboardings");
        script.attach(&http);
        io.on_header = [](char*, size_t size, void*) { return size; };
        io.on_write = [](char*, size_t size, void*) { return size; };
        io.on_read = [this](char* buffer, size_t size, void*) {
            size_t n = body.size() - body_read;
            if (n > size) {
                n = size;
            }
            memcpy(buffer, body.data() + body_read, n);
            body_read += n;
            return n;
        };
        khc_set_cb_read(&http, khct::cb::cb_read, &io);
        khc_set_cb_write(&http, khct::cb::cb_write, &io);
        khc_set_cb_header(&http, khct::cb::cb_header, &io);
        khc_set_logger(&http, log.get());
    }

    ~Fixture() {
        khc_slist_free_all(headers);
    }

    void header(const std::string& line) {
        headers = khc_slist_append(headers, line.c_str(), line.size());
        khc_set_req_headers(&http, headers);
    }
};

}

#ifndef EBISU_NO_LOG
TEST_CASE( "khc logs each request and response, without credentials or bodies" ) {
    Fixture f;
    f.header("Authorization: Bearer the-access-token");
    f.header("Content-Type: application/json");
    f.header("Cookie: session=the-cookie");
    f.body = "{\"thingPassword\":\"the-password\"}";
    khc_set_req_body_length(&f.http, f.body.size());
    const std::string resp_body = "{\"accessToken\":\"the-new-token\"}";
    f.script.responses = { "HTTP/1.1 200 OK\r\n"
        "Set-Cookie: session=the-new-cookie\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(resp_body.size()) + "\r\n"
        "\r\n" + resp_body };

    REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );

    const std::vector<std::pair<ebisu_log_level_t, std::string>> expected = {
        { EBISU_LOG_DEBUG, "connected to api.kii.com:443" },
        { EBISU_LOG_DEBUG, "> POST /api/apps/app1/onboardings HTTP/1.1" },
        { EBISU_LOG_DEBUG, "> HOST: api.kii.com" },
        { EBISU_LOG_DEBUG, "> Authorization: Bearer ***" },
        { EBISU_LOG_DEBUG, "> Content-Type: application/json" },
        { EBISU_LOG_DEBUG, "> Cookie: ***" },
        { EBISU_LOG_DEBUG, "> Content-Length: " + std::to_string(f.body.size()) },
        { EBISU_LOG_DEBUG, "> Connection: Close" },
        { EBISU_LOG_DEBUG, "> (a body of " + std::to_string(f.body.size()) + " bytes)" },
        { EBISU_LOG_DEBUG, "< HTTP/1.1 200 OK" },
        { EBISU_LOG_DEBUG, "< Set-Cookie: ***" },
        { EBISU_LOG_DEBUG, "< Content-Type: application/json" },
        { EBISU_LOG_DEBUG, "< Content-Length: " + std::to_string(resp_body.size()) },
        { EBISU_LOG_DEBUG, "< (a body of " + std::to_string(resp_body.size()) + " bytes)" },
    };
    CHECK( f.log.lines == expected );
    for (const char* secret : { "the-access-token", "the-cookie", "the-password",
            "the-new-cookie", "the-new-token" }) {
        INFO( secret );
        CHECK_FALSE( f.log.mentions(secret) );
    }
}

TEST_CASE( "khc logs credentials of any form as hidden" ) {
    Fixture f;
    khc_set_method(&f.http, "GET");
    f.script.responses = { khct::http_response(200, "{}") };

    SECTION("in a value of one word, which may be them") {
        f.header("Authorization: the-credentials");
        REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> Authorization: ***") );
    }

    SECTION("in a header named in another case, or for a proxy") {
        f.header("authorization: basic the-credentials");
        f.header("PROXY-AUTHORIZATION:Basic   the-credentials");
        f.header("set-cookie: the-credentials");
        REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> authorization: basic ***") );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> PROXY-AUTHORIZATION: Basic ***") );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> set-cookie: ***") );
    }

    SECTION("but not in a header whose name starts like one, or is the start of one") {
        f.header("Authorization-Hint: shown");
        f.header("Auth: shown");
        f.header("Cook: shown");
        REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> Authorization-Hint: shown") );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> Auth: shown") );
        CHECK( f.log.has(EBISU_LOG_DEBUG, "> Cook: shown") );
    }

    CHECK_FALSE( f.log.mentions("the-credentials") );
}

TEST_CASE( "khc logs how long a body of unknown length was, in chunks both ways" ) {
    Fixture f;
    f.body = std::string(3000, 'b');
    khc_set_req_body_length(&f.http, KHC_REQ_BODY_LENGTH_UNKNOWN);
    f.script.responses = { "HTTP/1.1 200 OK\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "6\r\n world\r\n"
        "0\r\n\r\n" };

    REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
    CHECK( f.log.has(EBISU_LOG_DEBUG, "> Transfer-Encoding: chunked") );
    CHECK( f.log.has(EBISU_LOG_DEBUG, "> (a body of 3000 bytes)") );
    CHECK( f.log.has(EBISU_LOG_DEBUG, "< Transfer-Encoding: chunked") );
    CHECK( f.log.has(EBISU_LOG_DEBUG, "< (a body of 11 bytes)") );
    CHECK_FALSE( f.log.mentions("hello") );
}

TEST_CASE( "khc logs a request that failed, and a connection that didn't close" ) {
    Fixture f;
    khc_set_method(&f.http, "GET");

    SECTION("connecting") {
        // No response for the connection, which refuses it.
        REQUIRE( khc_perform(&f.http) == KHC_ERR_SOCK_CONNECT );
        CHECK( f.log.lines == std::vector<std::pair<ebisu_log_level_t, std::string>>{
            { EBISU_LOG_WARNING, "GET api.kii.com/api/apps/app1/onboardings failed: KHC_ERR_SOCK_CONNECT" },
        } );
    }

    SECTION("with a response it can't read") {
        f.script.responses = { "HTTP/1.1 200 OK\r\nContent-Length: x\r\n\r\n" };
        REQUIRE( khc_perform(&f.http) == KHC_ERR_INVALID_RESPONSE );
        CHECK( f.log.lines.back() == std::make_pair(EBISU_LOG_WARNING,
                std::string("GET api.kii.com/api/apps/app1/onboardings failed: KHC_ERR_INVALID_RESPONSE")) );
    }

    SECTION("closing, after a response read whole") {
        f.script.responses = { khct::http_response(204, "") };
        f.script.ctx.on_close = [](void*) { return EBISU_SOCK_FAIL; };
        REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.log.lines.back() == std::make_pair(EBISU_LOG_WARNING,
                std::string("closing the connection to api.kii.com failed")) );
    }
}

#else
// Built without logging, khc logs nothing, whatever its logger.
TEST_CASE( "khc built without logging logs nothing" ) {
    Fixture f;
    f.header("Authorization: Bearer the-access-token");
    f.body = "{}";
    khc_set_req_body_length(&f.http, f.body.size());
    f.script.responses = { khct::http_response(200, "{}") };
    REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
    // No response for the next connection, which refuses it.
    REQUIRE( khc_perform(&f.http) == KHC_ERR_SOCK_CONNECT );
    CHECK( f.log.lines.empty() );
}
#endif

TEST_CASE( "khc logs to the logger it is given, whose setting the resets keep" ) {
    Fixture f;
    khc_set_method(&f.http, "GET");
    f.script.responses = { khct::http_response(200, "{}"), khct::http_response(200, "{}") };

    SECTION("none, by khc_init()") {
        khc_init(&f.http);
        CHECK( f.http._logger == nullptr );
    }

    SECTION("after khc_reset_except_cb()") {
        khc_reset_except_cb(&f.http);
        CHECK( f.http._logger == f.log.get() );
    }

    SECTION("or none, which logs nothing") {
        khc_set_logger(&f.http, nullptr);
        REQUIRE( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.log.lines.empty() );
    }
}
