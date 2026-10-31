#include "catch.hpp"
#include "kii.h"
#include "ebisu_version.h"
#include "test_callbacks.h"

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
        return KHC_SOCK_OK;
    };
    http_ctx.on_send = [&requests](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        requests.back().append(buffer, length);
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

// A failure to close was reported as a failure to connect, so
// KII_ERR_SOCK_CLOSE and TIO_ERR_SOCK_CLOSE were never returned.
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
        { "close", KII_ERR_SOCK_CLOSE },
    };
    for (const auto& c : cases) {
        INFO("failing " << c.failing);
        const std::string failing = c.failing;
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
            return failing == "recv" ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };
        http_ctx.on_close = [&failing](void*) {
            return failing == "close" ? KHC_SOCK_FAIL : KHC_SOCK_OK;
        };

        REQUIRE( kii_api_call_start(&kii, "GET", "/api/x", NULL, KII_FALSE) == KII_ERR_OK );
        CHECK( kii_api_call_run(&kii) == c.expected );
    }
}
