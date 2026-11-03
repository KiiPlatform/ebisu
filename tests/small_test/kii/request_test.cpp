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
                kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, true);
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
        kii_set_use_m_0_header_flag(&kii, true);
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

        REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, false) == KII_ERR_OK );
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

    REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, false) == KII_ERR_OK );
    CHECK( kii_api_call_run(&kii) == KII_ERR_TOO_LARGE_DATA );

    SECTION("a callback the application gave keeps its own failure") {
        kii_bucket_t bucket = { KII_SCOPE_THING, "th.1", "b" };
        int calls = 0;
        CHECK( kii_download_object_body(&kii, &bucket, "o", refuse_body, &calls) == KII_ERR_WRITE_CALLBACK );
        CHECK( calls == 1 );

        // And kii's is back for the next request.
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, false) == KII_ERR_OK );
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

    REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, false) == KII_ERR_OK );

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
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, false) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( request.find("GET /api/x HTTP/1.1\r\n") == 0 );
        CHECK( count(request, "HOST: api.kii.com\r\n") == 1 );
    }

    SECTION("when asked") {
        kii_enable_absolute_target(&kii, true);
        for (int i = 0; i < 2; ++i) {
            INFO("request " << i);
            REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, false) == KII_ERR_OK );
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
        CHECK( kii_api_call_start(&kii, "GET", path.c_str(), NULL, false) == KII_ERR_TOO_LARGE_DATA );
    }

    SECTION("the method of kii_api_call_start") {
        const std::string method(KHC_METHOD_MAX_SIZE, 'M');
        CHECK( kii_api_call_start(&kii, method.c_str(), "/api/x", NULL, false) == KII_ERR_TOO_LARGE_DATA );
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
        REQUIRE( kii_api_call_start(&kii, "PUT", "/api/x", "application/json", false) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_body(&kii, body.c_str(), body.size()) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        const std::string& sent = script.requests[0];
        CHECK( count(sent, "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n") == 1 );
        CHECK( count(sent, "Transfer-Encoding") == 0 );
        CHECK( sent.substr(sent.size() - body.size()) == body );
    }

    SECTION("no body at all") {
        REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, false) == KII_ERR_OK );
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
        REQUIRE( kii_api_call_start(&kii, "PUT", "/api/x", "application/json", false) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_body(&kii, "{}", 2) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        khc_reset_except_request(&kii._khc);
        CHECK( kii._khc._req_body_length == 2 );
    }
}

TEST_CASE( "kii says why its HTTP socket failed" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);
    khct::HttpScript script;
    script.attach(&kii);
    // The connection is refused, and the socket says the host didn't resolve.
    script.ctx.on_last_error = [](long* out_detail) {
        *out_detail = -2;
        return EBISU_SOCK_ERROR_DNS;
    };
    kii_topic_t topic = { KII_SCOPE_THING, "th.1", "t" };
    CHECK( kii_put_topic(&kii, &topic) == KII_ERR_SOCK_CONNECT );
    long detail = 0;
    CHECK( kii_get_sock_error(&kii, &detail) == EBISU_SOCK_ERROR_DNS );
    CHECK( detail == -2 );
}

// A request called a socket operation that returned EBISU_SOCK_AGAIN again at
// once, and nothing bounded it as a whole. The system set by kii_set_system()
// now has it wait, and kii_set_request_timeout() bounds it.
TEST_CASE( "kii's requests wait for their socket, and time out" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");
    khct::HttpScript script;
    script.attach(&kii);
    script.responses = { khct::http_response(204, ""), khct::http_response(204, "") };

    // A clock the system's delay moves on, as the socket below may.
    uint64_t now = 1000;
    std::vector<uint32_t> delays;
    khct::cb::SystemCtx sys;
    sys.on_delay_ms = [&now, &delays](uint32_t ms) {
        delays.push_back(ms);
        now += ms;
    };
    sys.on_monotonic_ms = [&now]() { return now; };
    kii_topic_t topic = { KII_SCOPE_THING, "th.1", "t" };

    SECTION("a socket not ready is waited for with the system's delay") {
        int agains = 2;
        auto connect = script.ctx.on_connect;
        script.ctx.on_connect = [&agains, connect](void* ctx, const char* host, uint16_t port) {
            return agains-- > 0 ? EBISU_SOCK_AGAIN : connect(ctx, host, port);
        };
        kii_set_system(&kii, sys.system());
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_OK );
        CHECK( delays == std::vector<uint32_t>(2, KHC_AGAIN_WAIT_MS) );
    }

    SECTION("a response trickling in past the timeout is KII_ERR_TIMEOUT") {
        kii_set_system(&kii, sys.system());
        REQUIRE( kii_set_request_timeout(&kii, 1) == KII_ERR_OK );
        auto recv = script.ctx.on_recv;
        script.ctx.on_recv = [&now, recv](void* ctx, char* buffer, size_t, size_t* out_actual_length) {
            now += 100;
            return recv(ctx, buffer, 1, out_actual_length);
        };
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_TIMEOUT );
        CHECK( kii_get_resp_status(&kii) == 0 );
    }

    SECTION("each request is timed from its own start") {
        kii_set_system(&kii, sys.system());
        REQUIRE( kii_set_request_timeout(&kii, 1) == KII_ERR_OK );
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_OK );
        now += 5000;
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_OK );
    }

    SECTION("a timeout without a clock sends nothing") {
        REQUIRE( kii_set_request_timeout(&kii, 1) == KII_ERR_OK );
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_FAIL );
        CHECK( script.requests.empty() );
    }

    SECTION("the timeouts it takes") {
        CHECK( kii_set_request_timeout(&kii, 4294967) == KII_ERR_OK );
        CHECK( kii._khc._timeout_ms == 4294967000U );
        CHECK( kii_set_request_timeout(&kii, 4294968) == KII_ERR_FAIL );
        CHECK( kii._khc._timeout_ms == 4294967000U );
        CHECK( kii_set_request_timeout(&kii, 0) == KII_ERR_OK );
        CHECK( kii._khc._timeout_ms == 0 );
    }
}

namespace {

// Gives body, at most size at a time, in pieces of at most piece if set.
struct BodyReader {
    std::string body;
    size_t pos = 0;
    size_t piece = 0;
    int calls = 0;
};

size_t read_body(char* buffer, size_t size, void* userdata)
{
    BodyReader* r = (BodyReader*)userdata;
    ++r->calls;
    size_t n = r->body.size() - r->pos;
    if (n > size) { n = size; }
    if (r->piece > 0 && n > r->piece) { n = r->piece; }
    memcpy(buffer, r->body.data() + r->pos, n);
    r->pos += n;
    return n;
}

// The body of a request, after its headers, decoded if chunked.
std::string body_of(const std::string& request)
{
    size_t start = request.find("\r\n\r\n");
    REQUIRE( start != std::string::npos );
    std::string rest = request.substr(start + 4);
    if (request.find("Transfer-Encoding: chunked\r\n") == std::string::npos) {
        return rest;
    }
    std::string body;
    size_t pos = 0;
    while (true) {
        size_t line_end = rest.find("\r\n", pos);
        REQUIRE( line_end != std::string::npos );
        size_t size = std::stoul(rest.substr(pos, line_end - pos), nullptr, 16);
        if (size == 0) {
            return body;
        }
        body += rest.substr(line_end + 2, size);
        pos = line_end + 2 + size + 2;
    }
}

}

// kii_api_call_append_body() copied the whole body into the buffer, which
// also takes the response: a large body needed a large buffer for every kii_t.
// One read through a callback never has to be held whole.
TEST_CASE( "An API call's body read through a callback" ) {
    char kii_buff[256];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);
    khct::HttpScript script;
    script.attach(&kii);
    script.responses = {
        khct::http_response(200, "{\"ok\":true}"),
        khct::http_response(200, "{\"ok\":true}"),
    };
    // Several times the buffer, with a byte of each value.
    BodyReader reader;
    for (int i = 0; i < 4; ++i) {
        for (int c = 0; c < 256; ++c) {
            reader.body += (char)c;
        }
    }
    auto start = [&kii]() {
        REQUIRE( kii_api_call_start(&kii, "POST", "/api/apps/dummyAppID/big",
                    "application/octet-stream", false) == KII_ERR_OK );
    };

    SECTION("with its length, whole, and the response in the buffer") {
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, reader.body.size()) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        REQUIRE( script.requests.size() == 1 );
        CHECK( khct::contains(script.requests[0], "Content-Length: 1024\r\n") );
        CHECK_FALSE( khct::contains(script.requests[0], "Transfer-Encoding") );
        CHECK( body_of(script.requests[0]) == reader.body );
        CHECK( kii_get_resp_status(&kii) == 200 );
        CHECK( std::string(kii_buff, kii_get_resp_body_length(&kii)) == "{\"ok\":true}" );
    }

    SECTION("chunked, without one") {
        reader.piece = 100;
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, KHC_REQ_BODY_LENGTH_UNKNOWN) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        REQUIRE( script.requests.size() == 1 );
        CHECK( khct::contains(script.requests[0], "Transfer-Encoding: chunked\r\n") );
        CHECK( body_of(script.requests[0]) == reader.body );
        CHECK( reader.calls > 10 );
    }

    SECTION("ending before its length fails the call") {
        reader.body.resize(1000);
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, 1024) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_FAIL );
    }

    SECTION("is the call's alone: the next takes its body from the buffer") {
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, reader.body.size()) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        int calls = reader.calls;
        start();
        REQUIRE( kii_api_call_append_body(&kii, "{\"a\":1}", 7) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        REQUIRE( script.requests.size() == 2 );
        CHECK( body_of(script.requests[1]) == "{\"a\":1}" );
        CHECK( khct::contains(script.requests[1], "Content-Length: 7\r\n") );
        CHECK( reader.calls == calls );
    }

    SECTION("is the call's alone, though it isn't run") {
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, reader.body.size()) == KII_ERR_OK );
        start();
        REQUIRE( kii_api_call_append_body(&kii, "{\"a\":1}", 7) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( body_of(script.requests[0]) == "{\"a\":1}" );
        CHECK( reader.calls == 0 );
    }

    SECTION("in place of an appended body, not as well") {
        start();
        REQUIRE( kii_api_call_append_body(&kii, "{}", 2) == KII_ERR_OK );
        CHECK( kii_api_call_set_body_reader(&kii, read_body, &reader, 2) == KII_ERR_FAIL );
        start();
        REQUIRE( kii_api_call_set_body_reader(&kii, read_body, &reader, 2) == KII_ERR_OK );
        CHECK( kii_api_call_append_body(&kii, "{}", 2) == KII_ERR_FAIL );
        start();
        CHECK( kii_api_call_set_body_reader(&kii, NULL, NULL, 2) == KII_ERR_FAIL );
    }
}
