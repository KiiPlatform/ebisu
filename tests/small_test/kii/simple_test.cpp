#include "catch.hpp"
#include "kii.h"
#include "kii_mqtt_task.h"
#include "test_callbacks.h"
#include "http_script.h"

#include <sstream>
#include <string>
#include <string.h>

TEST_CASE( "Simple test" ) {
    size_t kii_buff_size = 1024;
    char kii_buff[kii_buff_size];
    size_t mqtt_buff_size = 1024;
    char mqtt_buff[kii_buff_size];
    jkii_token_t jkii_tokens[256];
    jkii_resource_t jkii_resource = {jkii_tokens, 256};

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, kii_buff_size);
    kii_set_mqtt_buff(&kii, mqtt_buff, mqtt_buff_size);
    kii_set_json_parser_resource(&kii, &jkii_resource);
    kii._keep_alive_interval = 300;

    khct::cb::SockCtx http_ctx;
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    khct::cb::SockCtx mqtt_ctx;
    kii_set_cb_mqtt_sock_connect(&kii, khct::cb::mock_connect, &mqtt_ctx);
    kii_set_cb_mqtt_sock_send(&kii, khct::cb::mock_send, &mqtt_ctx);
    kii_set_cb_mqtt_sock_recv(&kii, khct::cb::mock_recv, &mqtt_ctx);
    kii_set_cb_mqtt_sock_close(&kii, khct::cb::mock_close, &mqtt_ctx);

    kii_set_cb_delay_ms(&kii, khct::cb::cb_delay_ms, NULL);

    khct::cb::PushCtx push_ctx;
    kii._cb_push_received = khct::cb::cb_push;
    kii._push_data = &push_ctx;

    SECTION("kii_enable_insecure_http test") {
        REQUIRE(kii._khc._enable_insecure == 0);
        kii_enable_insecure_http(&kii, KII_TRUE);
        REQUIRE(kii._khc._enable_insecure == 1);
        kii_enable_insecure_http(&kii, KII_FALSE);
        REQUIRE(kii._khc._enable_insecure == 0);
    }

    SECTION("kii_set_use_m_0 test") {
        REQUIRE(kii._use_m_0_header == KII_FALSE);
        kii_set_use_m_0_header_flag(&kii, KII_TRUE);
        REQUIRE(kii._use_m_0_header == KII_TRUE);
        kii_set_use_m_0_header_flag(&kii, KII_FALSE);
        REQUIRE(kii._use_m_0_header == KII_FALSE);
    }
}

// kii_set_app_id and kii_set_site used to fill their fixed-size fields with
// strncpy(dest, src, sizeof(dest)). strncpy writes no terminator when the
// source is at least as long as the count, so input at or over the field size
// left the field unterminated and every later strlen over it ran off the end.
TEST_CASE( "Over-long app id and site are truncated and stay terminated" ) {
    kii_t kii;
    kii_init(&kii);

    const std::string long_app_id(KII_APP_ID_MAX_SIZE + 64, 'a');
    kii_set_app_id(&kii, long_app_id.c_str());
    REQUIRE( strlen(kii._app_id) == KII_APP_ID_MAX_SIZE - 1 );

    const std::string long_site(KII_APP_HOST_MAX_SIZE + 64, 'h');
    kii_set_site(&kii, long_site.c_str());
    REQUIRE( strlen(kii._app_host) == KII_APP_HOST_MAX_SIZE - 1 );

    // Exactly one byte too long: the boundary the old code got wrong.
    const std::string exact(KII_APP_ID_MAX_SIZE, 'b');
    kii_set_app_id(&kii, exact.c_str());
    REQUIRE( strlen(kii._app_id) == KII_APP_ID_MAX_SIZE - 1 );

    // Something that fits is copied whole.
    kii_set_app_id(&kii, "dummyAppID");
    REQUIRE( std::string(kii._app_id) == "dummyAppID" );
}

// Every response header line goes through the ETag parser, up to the size of the
// response header buffer, so a long one must not cost its length in stack.
TEST_CASE( "The ETag is read from the response, whatever the other headers" ) {
    kii_t kii;
    kii_init(&kii);
    char kii_buff[1024];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_site(&kii, "api.kii.com");
    static char resp_header_buff[8192];
    kii_set_resp_header_buff(&kii, resp_header_buff, sizeof(resp_header_buff));

    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
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

    const std::string cookie = "Set-Cookie: c=" + std::string(8000, 'x') + "\r\n";
    resp = "HTTP/1.1 200 OK\r\n" + cookie +
        "ETag: \"1\"\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "\"1\"" );

    // In any case, and with whitespace around the value.
    resp = "HTTP/1.1 200 OK\r\n"
        "etag:  \"2\" \r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "\"2\"" );

    resp = "HTTP/1.1 200 OK\r\n"
        "Content-Length: 2\r\n"
        "\r\n"
        "{}";
    REQUIRE( send() == KII_ERR_OK );
    CHECK( std::string(kii_get_etag(&kii)) == "" );
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
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);

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
            return failing == "connect" ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };
        http_ctx.on_send = [&failing](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return failing == "send" ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };
        const std::string resp = "HTTP/1.1 200 OK\r\n\r\n";
        size_t resp_pos = 0;
        http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
            resp_pos += *out_actual_length;
            return recv_fails ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };
        http_ctx.on_close = [close_fails](void*) {
            return close_fails ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };

        REQUIRE( kii_api_call_start(&kii, "POST", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == c.expected );
    }
}


static size_t count(const std::string& s, const std::string& what) {
    size_t n = 0;
    for (size_t pos = s.find(what); pos != std::string::npos; pos = s.find(what, pos + 1)) {
        ++n;
    }
    return n;
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
    http_ctx.on_connect = [](void*, const char*, unsigned int) { return KHC_SOCK_OK; };
    http_ctx.on_send = [&request](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        request.append(buffer, length);
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    const std::string resp = "HTTP/1.1 204 No Content\r\n\r\n";
    size_t resp_pos = 0;
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);

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
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);

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
        // 2.x sends the whole URL in the request line.
        CHECK( script.requests[0].find("POST https://api.kii.com" + prefix + endpoint + " HTTP/1.1\r\n") == 0 );
    }
}


// It was KII_ERR_FAIL for a status line, and KII_ERR_HEADER_CALLBACK for
// framing, though no callback had failed.
TEST_CASE( "A response khc can't read is KII_ERR_INVALID_RESPONSE" ) {
    char kii_buff[1024];
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "tok");
    khct::HttpScript script;
    script.attach(&kii);
    kii_topic_t topic = { KII_SCOPE_THING, "th.1", "t" };
    const std::string responses[] = {
        "HTTP/1.1 abc OK\r\n\r\n",
        "SIP/2.0 200 OK\r\n\r\n",
        "HTTP/1.1 204 No Content\r\nContent-Length: 1x\r\n\r\n",
        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 3\r\n\r\nabc",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip\r\n\r\nabc",
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nabc\r\n0\r\n\r\n",
    };
    for (const auto& response : responses) {
        INFO( response );
        script.responses = { response };
        script.requests.clear();
        CHECK( kii_put_topic(&kii, &topic) == KII_ERR_INVALID_RESPONSE );
    }
}

// kii_get_etag() gives kii_t's own copy, which every call emptied as it
// started: before it read the ETag it was given. So an update written as the
// README has it, kii_put_object(..., kii_get_etag(&kii)), sent no If-Match,
// and went through whatever had become of the object.
TEST_CASE( "The last response's ETag can be given as it is to the next request" ) {
    kii_t kii;
    kii_init(&kii);
    char kii_buff[4096];
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_site(&kii, "api.kii.com");

    std::string resp;
    size_t resp_pos = 0;
    std::string sent;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos, &sent](void*, const char*, unsigned int) {
        resp_pos = 0;
        sent.clear();
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [&sent](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
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

    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.bucket_name = "b";
    bucket.scope_id = "th.1";
    auto respond = [&resp](int status, const std::string& headers, const std::string& body) {
        resp = "HTTP/1.1 " + std::to_string(status) + " Status\r\n" + headers +
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    };
    const std::string object = "{\"a\":1}";
    const std::string if_match = "\r\nIf-Match: \"1\"\r\n";

    respond(200, "ETag: \"1\"\r\n", object);
    REQUIRE( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_OK );
    REQUIRE( std::string(kii_get_etag(&kii)) == "\"1\"" );

    SECTION("to kii_put_object()") {
        respond(200, "ETag: \"2\"\r\n", "");
        CHECK( kii_put_object(&kii, &bucket, "o.1", object.c_str(), NULL, kii_get_etag(&kii)) ==
                KII_ERR_OK );
        CHECK( sent.find(if_match) != std::string::npos );
        // That response's, from then on.
        CHECK( std::string(kii_get_etag(&kii)) == "\"2\"" );
    }
    SECTION("to kii_patch_object()") {
        respond(200, "ETag: \"2\"\r\n", "");
        CHECK( kii_patch_object(&kii, &bucket, "o.1", object.c_str(), kii_get_etag(&kii)) ==
                KII_ERR_OK );
        CHECK( sent.find(if_match) != std::string::npos );
        CHECK( std::string(kii_get_etag(&kii)) == "\"2\"" );
    }
    SECTION("to kii_api_call_append_header(), once the call is started") {
        respond(200, "", "{}");
        REQUIRE( kii_api_call_start(&kii, "PUT", "/api/apps/dummyAppID/x", "application/json",
                    KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_header(&kii, "If-Match", kii_get_etag(&kii)) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( sent.find(if_match) != std::string::npos );
        // A response without one leaves none of the one before.
        CHECK( std::string(kii_get_etag(&kii)) == "" );
    }
    SECTION("a request that fails once made leaves none either") {
        http_ctx.on_connect = [](void*, const char*, unsigned int) { return KHC_SOCK_FAIL; };
        CHECK( kii_get_object(&kii, &bucket, "o.1") != KII_ERR_OK );
        CHECK( std::string(kii_get_etag(&kii)) == "" );
    }
    SECTION("a call that fails before its request is made leaves it, for the next") {
        const std::string id(8192, 'o');
        CHECK( kii_put_object(&kii, &bucket, id.c_str(), object.c_str(), NULL, kii_get_etag(&kii)) ==
                KII_ERR_TOO_LARGE_DATA );
        CHECK( std::string(kii_get_etag(&kii)) == "\"1\"" );
        respond(200, "ETag: \"2\"\r\n", "");
        CHECK( kii_put_object(&kii, &bucket, "o.1", object.c_str(), NULL, kii_get_etag(&kii)) ==
                KII_ERR_OK );
        CHECK( sent.find(if_match) != std::string::npos );
    }
}


namespace {

size_t append_to_string(char* buffer, size_t size, void* userdata) {
    ((std::string*)userdata)->append(buffer, size);
    return size;
}

}

// A download handed every response's body to the caller's callback: an
// error's went where the object's body was to go.
TEST_CASE( "A download gives its callback the object's body alone" ) {
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
    khct::HttpScript http;
    http.attach(&kii);

    kii_bucket_t bucket;
    bucket.scope = KII_SCOPE_THING;
    bucket.scope_id = "th.1";
    bucket.bucket_name = "b";
    std::string sink;

    SECTION("an error's is read into kii's buffer") {
        const std::string error = "{\"errorCode\":\"OBJECT_BODY_NOT_FOUND\"}";
        http.responses = { khct::http_response(404, error) };
        CHECK( kii_download_object_body(&kii, &bucket, "o", append_to_string, &sink) ==
                KII_ERR_RESP_STATUS );
        CHECK( kii_get_resp_status(&kii) == 404 );
        CHECK( sink == "" );
        CHECK( std::string(kii_buff, kii_get_resp_body_length(&kii)) == error );
    }
    SECTION("an error's longer than the buffer is cut, and still an error") {
        http.responses = { khct::http_response(500, std::string(3 * sizeof(kii_buff), 'e')) };
        CHECK( kii_download_object_body(&kii, &bucket, "o", append_to_string, &sink) ==
                KII_ERR_RESP_STATUS );
        CHECK( kii_get_resp_status(&kii) == 500 );
        CHECK( sink == "" );
        CHECK( kii_get_resp_body_length(&kii) <= sizeof(kii_buff) );
    }
    SECTION("the object's goes to the callback, whatever its length") {
        const std::string body(3 * sizeof(kii_buff), 'o');
        http.responses = { khct::http_response(200, body) };
        CHECK( kii_download_object_body(&kii, &bucket, "o", append_to_string, &sink) == KII_ERR_OK );
        CHECK( sink == body );
        // And kii's own callback is back for the next call.
        http.responses.push_back(khct::http_response(200, "{}"));
        CHECK( kii_get_object(&kii, &bucket, "o") == KII_ERR_OK );
        CHECK( sink == body );
        CHECK( std::string(kii_buff, kii_get_resp_body_length(&kii)) == "{}" );
    }
}

// The body is read by its length, and nothing says it is a string. But code
// that reads it as one works for every body shorter than the buffer, which is
// zeroed first, and read past the buffer for the one that filled it exactly.
// And after a response with no body, it found the request's.
TEST_CASE( "The byte after a response's body is a NUL, whatever fits" ) {
    char memory[64 + 1];
    char* kii_buff = memory;
    const size_t buff_size = 64;
    // What a string read past the buffer would run into.
    memory[buff_size] = 'X';

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "a");
    kii_set_buff(&kii, kii_buff, buff_size);

    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&resp_pos](void*, const char*, unsigned int) {
        resp_pos = 0;
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return KHC_SOCK_OK;
    };
    http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
    kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
    kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
    kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    auto respond = [&resp](const std::string& body) {
        resp = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    };
    auto get = [&kii]() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/x", NULL, KII_FALSE);
        return res == KII_ERR_OK ? kii_api_call_run(&kii) : res;
    };

    SECTION("one byte shorter than the buffer is the longest it takes") {
        const std::string body(buff_size - 1, 'b');
        respond(body);
        REQUIRE( get() == KII_ERR_OK );
        REQUIRE( kii_get_resp_body_length(&kii) == body.size() );
        CHECK( std::string(kii_buff, body.size()) == body );
        CHECK( kii_buff[body.size()] == '\0' );
        CHECK( strlen(kii_buff) == body.size() );
    }
    SECTION("one that would fill it is too large") {
        respond(std::string(buff_size, 'b'));
        CHECK( get() == KII_ERR_TOO_LARGE_DATA );
        CHECK( memory[buff_size] == 'X' );
    }
    SECTION("in pieces, as chunks come") {
        resp = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "20\r\n" + std::string(32, 'c') + "\r\n1f\r\n" + std::string(31, 'd') + "\r\n0\r\n\r\n";
        REQUIRE( get() == KII_ERR_OK );
        REQUIRE( kii_get_resp_body_length(&kii) == 63 );
        CHECK( strlen(kii_buff) == 63 );
        resp = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"
            "20\r\n" + std::string(32, 'c') + "\r\n20\r\n" + std::string(32, 'd') + "\r\n0\r\n\r\n";
        CHECK( get() == KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("with no body, after a request that had one") {
        resp = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";
        REQUIRE( kii_api_call_start(&kii, "PUT", "/x", "application/json", KII_FALSE) == KII_ERR_OK );
        REQUIRE( kii_api_call_append_body(&kii, "{\"sent\":1}", 10) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        CHECK( kii_get_resp_body_length(&kii) == 0 );
        CHECK( kii_buff[0] == '\0' );
    }
    SECTION("with a shorter body after a longer one") {
        respond(std::string(40, 'l'));
        REQUIRE( get() == KII_ERR_OK );
        respond("short");
        REQUIRE( get() == KII_ERR_OK );
        CHECK( std::string(kii_buff) == "short" );
    }
}


namespace {

size_t read_nothing(char*, size_t, void*) {
    return 0;
}

}

// What goes into a request's path or a header is sent as it is. A space in a
// bucket's or a topic's name, a CR and an LF in a header's value, made the
// request another one: one whose path ended early, or with a header of the
// caller's choosing, or two requests.
TEST_CASE( "What would break a request's line or a header fails the call, sending nothing" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "app");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);
    strcpy(kii._author.author_id, "th.1");
    strcpy(kii._author.access_token, "at1");
    khct::HttpScript http;
    http.attach(&kii);

    SECTION("a path of the application's") {
        CHECK( kii_api_call_start(&kii, "GET", "/api/apps/app/x HTTP/1.1\r\nX: y\r\n\r\nGET /z", NULL, KII_FALSE) ==
                KII_ERR_FAIL );
        CHECK( kii_api_call_start(&kii, "GET", "/api/apps/app/a b", NULL, KII_FALSE) == KII_ERR_FAIL );
        CHECK( kii_api_call_start(&kii, "GET X", "/api/apps/app/x", NULL, KII_FALSE) == KII_ERR_FAIL );
        // Too long is still that.
        const std::string long_path(KHC_PATH_MAX_SIZE, 'p');
        CHECK( kii_api_call_start(&kii, "GET", long_path.c_str(), NULL, KII_FALSE) == KII_ERR_TOO_LARGE_DATA );
    }
    SECTION("a header of the application's") {
        REQUIRE( kii_api_call_start(&kii, "GET", "/api/apps/app/x", NULL, KII_FALSE) == KII_ERR_OK );
        CHECK( kii_api_call_append_header(&kii, "X-A", "1\r\nAuthorization: Bearer other") == KII_ERR_FAIL );
        CHECK( kii_api_call_append_header(&kii, "X-A\n", "1") == KII_ERR_FAIL );
        CHECK( kii_api_call_append_header(&kii, "X-A", "1\r") == KII_ERR_FAIL );
        CHECK( kii._req_headers == nullptr );
    }
    SECTION("a bucket's name") {
        kii_bucket_t bucket;
        bucket.scope = KII_SCOPE_APP;
        bucket.scope_id = NULL;
        bucket.bucket_name = "b HTTP/1.1";
        CHECK( kii_get_object(&kii, &bucket, "o.1") == KII_ERR_FAIL );
        CHECK( kii_subscribe_bucket(&kii, &bucket) == KII_ERR_FAIL );
    }
    SECTION("a topic's name") {
        kii_topic_t topic;
        topic.scope = KII_SCOPE_APP;
        topic.scope_id = NULL;
        topic.topic_name = "t\r\nX: y";
        CHECK( kii_subscribe_topic(&kii, &topic) == KII_ERR_FAIL );
    }
    SECTION("a normalizer's host") {
        CHECK( kii_ti_put_state(&kii, read_nothing, NULL, NULL, NULL, "h.example\r\nX: y") ==
                KII_ERR_FAIL );
    }
    CHECK( http.requests.empty() );
    // And the next call goes as it should.
    http.responses = { khct::http_response(204, "") };
    CHECK( kii_ti_put_firmware_version(&kii, "1.0") == KII_ERR_OK );
    REQUIRE( http.requests.size() == 1 );
    // 2.x sends the whole URL in the request line.
    CHECK( khct::contains(http.requests[0],
                "PUT https://api.kii.com/thing-if/apps/app/things/th.1/firmware-version HTTP/1.1\r\n") );
}
