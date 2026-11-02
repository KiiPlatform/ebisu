#include "catch.hpp"
#include "kii.h"
#include "ebisu_version.h"
#include "http_script.h"

#include <functional>
#include <string>
#include <string.h>
#include <vector>

static const char state[] = "{\"power\":true}";

static size_t read_state(char* buffer, size_t size, void* userdata) {
    size_t* pos = (size_t*)userdata;
    size_t n = strlen(state) - *pos;
    if (n > size) { n = size; }
    memcpy(buffer, state + *pos, n);
    *pos += n;
    return n;
}

static size_t count(const std::string& s, const std::string& what) {
    size_t n = 0;
    for (size_t pos = s.find(what); pos != std::string::npos; pos = s.find(what, pos + 1)) {
        ++n;
    }
    return n;
}

// Every request goes out through one place, which adds the headers all of them
// carry. One of each kind is sent here.
TEST_CASE( "Every request names the SDK and its version" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);

    // Each request opens its own connection, answered with resp, and what is
    // sent through it is kept apart from the other requests'.
    std::string resp;
    size_t resp_pos = 0;
    std::vector<std::string> requests;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        requests.push_back("");
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [&requests](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        requests.back().append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&resp, &resp_pos](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        size_t n = resp.size() - resp_pos;
        if (n > length_to_read) { n = length_to_read; }
        memcpy(buffer, resp.data() + resp_pos, n);
        resp_pos += n;
        *out_actual_length = n;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    kii_bucket_t bucket = { KII_SCOPE_THING, "th.1", "b" };
    kii_topic_t topic = { KII_SCOPE_THING, "th.1", "t" };
    kii_object_id_t object_id;
    size_t state_pos = 0;
    struct call {
        const char* name;
        const char* status;
        std::string body;
        std::function<kii_code_t()> send;
    };
    const std::vector<call> calls = {
        { "kii_auth_thing", "200 OK", "{\"id\":\"th.1\",\"access_token\":\"tok\"}",
            [&] { return kii_auth_thing(&kii, "vid", "pass"); } },
        { "kii_register_thing", "201 Created", "{\"_thingID\":\"th.1\",\"_accessToken\":\"tok\"}",
            [&] { return kii_register_thing(&kii, "vid", "type", "pass"); } },
        { "kii_ti_onboard", "200 OK", "{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}",
            [&] { return kii_ti_onboard(&kii, "vid", "pass", NULL, NULL, NULL, NULL); } },
        { "kii_ti_put_firmware_version", "204 No Content", "",
            [&] { return kii_ti_put_firmware_version(&kii, "1.0"); } },
        { "kii_ti_put_state", "204 No Content", "",
            [&] { return kii_ti_put_state(&kii, read_state, &state_pos, NULL, NULL, NULL); } },
        { "kii_post_object", "201 Created", "{\"objectID\":\"o.1\"}",
            [&] { return kii_post_object(&kii, &bucket, "{}", NULL, &object_id); } },
        { "kii_put_topic", "204 No Content", "",
            [&] { return kii_put_topic(&kii, &topic); } },
        { "kii_execute_server_code", "200 OK", "{}",
            [&] { return kii_execute_server_code(&kii, "ep", "{}"); } },
        { "kii_api_call_run", "200 OK", "{}",
            [&] {
                kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, KII_TRUE);
                return (res == KII_ERR_OK) ? kii_api_call_run(&kii) : res;
            } },
    };

    const std::string sdk = "\r\nX-Kii-SDK: sn=te;sv=" EBISU_VERSION_FULL "\r\n";
    const std::string m_0 = "\r\nkii-m: 0\r\n";
    size_t m_0_count = 0;
    SECTION("without kii-m") {
        m_0_count = 0;
    }
    SECTION("with kii-m") {
        kii_set_use_m_0_header_flag(&kii, KII_TRUE);
        m_0_count = 1;
    }
    for (const call& c : calls) {
        INFO( c.name );
        resp = std::string("HTTP/1.1 ") + c.status + "\r\n"
            "Content-Length: " + std::to_string(c.body.size()) + "\r\n"
            "\r\n" + c.body;
        requests.clear();

        REQUIRE( c.send() == KII_ERR_OK );

        REQUIRE( requests.size() == 1 );
        CHECK( count(requests[0], sdk) == 1 );
        CHECK( count(requests[0], m_0) == m_0_count );
    }
}

// Closing comes last, once the request has its result. A failure there replaced
// it, so a response read whole came back as a failure, and the caller of a POST
// the server had acted on would send it again.
TEST_CASE( "A request reports the socket operation that failed" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);

    khct::cb::SockCtx http_ctx;
    kii_set_http_socket(&kii, http_ctx.socket());

    const struct {
        const char* failing;
        kii_code_t expected;
    } cases[] = {
        { "connect", KII_ERR_SOCK_CONNECT },
        { "send", KII_ERR_SOCK_SEND },
        { "recv", KII_ERR_SOCK_RECV },
        { "close", KII_ERR_OK },
        { "recv and close", KII_ERR_SOCK_RECV },
    };
    for (const auto& c : cases) {
        INFO("failing " << c.failing);
        const std::string failing = c.failing;
        const bool recv_fails = failing.find("recv") != std::string::npos;
        const bool close_fails = failing.find("close") != std::string::npos;
        http_ctx.on_connect = [&failing](void*, const char*, unsigned int) {
            return failing == "connect" ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
        };
        http_ctx.on_send = [&failing](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return failing == "send" ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
        };
        const std::string resp = "HTTP/1.1 200 OK\r\n\r\n";
        size_t resp_pos = 0;
        http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
            resp_pos += *out_actual_length;
            return recv_fails ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
        };
        http_ctx.on_close = [close_fails](void*) {
            return close_fails ? EBISU_SOCK_FAIL : EBISU_SOCK_OK;
        };

        REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == c.expected );
    }
}

// Onboarding escapes the same value.
TEST_CASE( "kii_ti_put_firmware_version escapes the version" ) {
    char kii_buff[1024];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));

    std::string request;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [](void*, const char*, unsigned int) { return EBISU_SOCK_OK; };
    http_ctx.on_send = [&request](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        request.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    const std::string resp = "HTTP/1.1 204 No Content\r\n\r\n";
    size_t resp_pos = 0;
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    REQUIRE( kii_ti_put_firmware_version(&kii, "1.0\"\\") == KII_ERR_OK );
    CHECK( count(request, "{\"firmwareVersion\":\"1.0\\\"\\\\\"}") == 1 );
}

static size_t refuse_body(char*, size_t, void* userdata) {
    ++*(int*)userdata;
    return 0;
}

// The only reason kii's own write callback fails. An application's is another
// matter, and keeps its code.
TEST_CASE( "A response too large for the buffer is reported as such" ) {
    char kii_buff[256];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));

    const std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 1024\r\n\r\n" + std::string(1024, 'x');
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
    CHECK( kii_api_call_run(&kii) == KII_ERR_TOO_LARGE_DATA );

    SECTION("a callback the application gave keeps its own failure") {
        kii_bucket_t bucket = { KII_SCOPE_THING, "th.1", "b" };
        int calls = 0;
        CHECK( kii_download_object_body(&kii, &bucket, "o", refuse_body, &calls) == KII_ERR_WRITE_CALLBACK );
        CHECK( calls == 1 );

        // And kii's is back for the next request.
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_TOO_LARGE_DATA );
    }
}

// Put together on the stack of the task making the request, so its length can't
// be left to whatever the application passes.
TEST_CASE( "kii_api_call_append_header takes what fits in the buffer" ) {
    char kii_buff[256];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));

    REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );

    // "k: " and the terminator.
    const std::string largest(sizeof(kii_buff) - strlen("k: ") - 1, 'v');
    CHECK( kii_api_call_append_header(&kii, "k", largest.c_str()) == KII_ERR_OK );

    const std::string too_long = largest + "v";
    CHECK( kii_api_call_append_header(&kii, "k", too_long.c_str()) == KII_ERR_TOO_LARGE_DATA );
}

// Each request starts by resetting khc, which has to leave this as it is.
TEST_CASE( "kii_enable_absolute_target holds for every request" ) {
    char kii_buff[1024];

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));

    std::string request;
    const std::string resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        request.clear();
        resp_pos = 0;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_send = [&request](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        request.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
    kii_set_http_socket(&kii, http_ctx.socket());

    SECTION("not by default") {
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( request.find("GET /api/x HTTP/1.1\r\n") == 0 );
        CHECK( count(request, "HOST: api.kii.com\r\n") == 1 );
    }

    SECTION("when asked") {
        kii_enable_absolute_target(&kii, KII_TRUE);
        for (int i = 0; i < 2; ++i) {
            INFO("request " << i);
            REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
            REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
            CHECK( request.find("GET https://api.kii.com/api/x HTTP/1.1\r\n") == 0 );
        }
    }
}

// kii_api_call_start() and the server code call took no notice of
// khc_set_path() or khc_set_method() failing on one too long for it, and sent
// the request without it.
TEST_CASE( "A path or method too long for khc is refused rather than left out" ) {
    char kii_buff[4096];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    khct::HttpScript script;
    script.attach(&kii);
    script.responses = { "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}" };

    SECTION("the path of kii_api_call_start") {
        const std::string path = "/api/" + std::string(KHC_PATH_MAX_SIZE, 'p');
        CHECK( kii_api_call_start(&kii, "GET", path.c_str(), NULL, KII_FALSE) == KII_ERR_TOO_LARGE_DATA );
    }

    SECTION("the method of kii_api_call_start") {
        const std::string method(KHC_METHOD_MAX_SIZE, 'M');
        CHECK( kii_api_call_start(&kii, method.c_str(), "/api/x", NULL, KII_FALSE) == KII_ERR_TOO_LARGE_DATA );
    }

    SECTION("a server code endpoint") {
        const std::string endpoint(KHC_PATH_MAX_SIZE, 'e');
        CHECK( kii_execute_server_code(&kii, endpoint.c_str(), "{}") == KII_ERR_TOO_LARGE_DATA );
        CHECK( script.requests.empty() );
    }

    SECTION("but the longest that fit are sent") {
        const std::string prefix = "/api/apps/dummyAppID/server-code/versions/current/";
        const std::string endpoint(KHC_PATH_MAX_SIZE - 1 - prefix.size(), 'e');
        CHECK( kii_execute_server_code(&kii, endpoint.c_str(), "{}") == KII_ERR_OK );
        REQUIRE( script.requests.size() == 1 );
        CHECK( script.requests[0].find("POST " + prefix + endpoint + " HTTP/1.1\r\n") == 0 );
    }
}

namespace {

size_t count_in(const std::string& s, const std::string& what) {
    size_t n = 0;
    for (size_t pos = s.find(what); pos != std::string::npos; pos = s.find(what, pos + 1)) {
        ++n;
    }
    return n;
}

}

// Server code always went with the token of the thing kii acts as.
TEST_CASE( "Server code runs with the token given, or none" ) {
    char kii_buff[4096];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "thing-token");
    khct::HttpScript script;
    script.attach(&kii);
    const std::string ok = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
    script.responses = { ok, ok, ok };

    REQUIRE( kii_execute_server_code(&kii, "ep", "{}") == KII_ERR_OK );
    CHECK( count_in(script.requests[0], "Authorization: Bearer thing-token\r\n") == 1 );

    REQUIRE( kii_execute_server_code_with_token(&kii, NULL, "ep", "{}", "admin-token") == KII_ERR_OK );
    CHECK( script.requests[1].find("POST /api/apps/dummyAppID/server-code/versions/current/ep HTTP/1.1\r\n") == 0 );
    CHECK( count_in(script.requests[1], "Authorization: Bearer admin-token\r\n") == 1 );
    CHECK( count_in(script.requests[1], "Authorization") == 1 );

    REQUIRE( kii_execute_server_code_with_token(&kii, "v2", "ep", "{}", NULL) == KII_ERR_OK );
    CHECK( script.requests[2].find("POST /api/apps/dummyAppID/server-code/versions/v2/ep HTTP/1.1\r\n") == 0 );
    CHECK( count_in(script.requests[2], "Authorization") == 0 );
}

// Every body went chunked, which some intermediaries handle poorly, though kii
// had the length of the ones it holds whole in its buffer.
TEST_CASE( "A body kii holds whole goes with its length, a streamed one chunked" ) {
    char kii_buff[4096];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");
    khct::HttpScript script;
    script.attach(&kii);
    const std::string ok = "HTTP/1.1 204 No Content\r\n\r\n";
    script.responses = { ok, ok, ok };

    SECTION("a body appended to an API call") {
        const std::string body = "{\"k\":\"v\"}";
        REQUIRE( kii_api_call_start(&kii, "PUT", "/api/x", "application/json", KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_body(&kii, body.c_str(), body.size()) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        const std::string& sent = script.requests[0];
        CHECK( count(sent, "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n") == 1 );
        CHECK( count(sent, "Transfer-Encoding") == 0 );
        CHECK( sent.substr(sent.size() - body.size()) == body );
    }

    SECTION("no body at all") {
        REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        const std::string& sent = script.requests[0];
        CHECK( count(sent, "\r\nContent-Length: 0\r\n") == 1 );
        CHECK( sent.substr(sent.size() - 4) == "\r\n\r\n" );
    }

    SECTION("a state streamed through the application's callback") {
        size_t pos = 0;
        REQUIRE( kii_ti_put_state(&kii, read_state, &pos, NULL, NULL, NULL) == KII_ERR_OK );
        const std::string& sent = script.requests[0];
        CHECK( count(sent, "Transfer-Encoding: chunked\r\n") == 1 );
        CHECK( count(sent, "Content-Length") == 0 );
        CHECK( count(sent, std::string(state) + "\r\n0\r\n\r\n") == 1 );
    }

    SECTION("a request sent again after a renewal keeps its length") {
        // Without a session nothing is sent again: the length set for the
        // request survives khc_reset_except_request() regardless.
        REQUIRE( kii_api_call_start(&kii, "PUT", "/api/x", "application/json", KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_body(&kii, "{}", 2) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        khc_reset_except_request(&kii._khc);
        CHECK( kii._khc._req_body_length == 2 );
    }
}
