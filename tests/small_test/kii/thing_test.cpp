#include "catch.hpp"
#include "kii.h"
#include "picojson.h"
#include "test_callbacks.h"

#include <deque>
#include <functional>
#include <string>
#include <string.h>

static std::string ok_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

// The password was escaped with the size of the vendor thing ID's buffer passed
// as its own. jkii_escape_str writes nothing when told the buffer is too small,
// so a password longer than about twice the ID went out as whatever was on the
// stack.
TEST_CASE( "Thing requests send the values they are given" ) {
    char kii_buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);

    // Each request opens its own connection, which is answered with the next
    // response queued. Everything sent is captured so the body can be checked.
    std::deque<std::string> responses;
    std::string resp;
    size_t resp_pos = 0;
    std::string sent;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        if (responses.empty()) {
            return KHC_SOCK_FAIL;
        }
        resp = responses.front();
        responses.pop_front();
        resp_pos = 0;
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

    const char vendor_thing_id[] = "ab";
    // Far beyond twice the ID, and with characters JSON needs escaped.
    const char password[] = "a/password\"much-longer-than-the-vendor-thing-id";
    const std::string esc_password = "a\\/password\\\"much-longer-than-the-vendor-thing-id";

    // Six bytes each, which is more than the requests used to make room for.
    const char control_chars[] = "\x01\x02\x03\x04";
    const std::string esc_control_chars = "\\u0001\\u0002\\u0003\\u0004";

    SECTION("kii_auth_thing with control characters") {
        responses.push_back(ok_response("{\"id\":\"th.1\",\"access_token\":\"tok\"}"));

        kii_code_t res = kii_auth_thing(&kii, control_chars, control_chars);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"username\":\"VENDOR_THING_ID:" + esc_control_chars + "\"") != std::string::npos );
        REQUIRE( sent.find("\"password\":\"" + esc_control_chars + "\"") != std::string::npos );
    }

    SECTION("kii_register_thing with control characters") {
        responses.push_back(ok_response("{\"_thingID\":\"th.1\",\"_accessToken\":\"tok\"}"));

        kii_code_t res = kii_register_thing(&kii, control_chars, control_chars, control_chars);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("{\"_vendorThingID\":\"" + esc_control_chars + "\", "
                    "\"_thingType\":\"" + esc_control_chars + "\", "
                    "\"_password\":\"" + esc_control_chars + "\"}") != std::string::npos );
    }

    SECTION("kii_ti_onboard with control characters") {
        // 2.x onboards with an anonymous token, which it asks for first.
        responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
        responses.push_back(ok_response("{\"thingID\":\"th.1\",\"accessToken\":\"tok\"}"));

        kii_code_t res = kii_ti_onboard(&kii, control_chars, control_chars,
                control_chars, control_chars, control_chars, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("{\"vendorThingID\":\"" + esc_control_chars + "\","
                    "\"thingPassword\":\"" + esc_control_chars + "\","
                    "\"thingType\":\"" + esc_control_chars + "\","
                    "\"firmwareVersion\":\"" + esc_control_chars + "\","
                    "\"layoutPosition\":\"" + esc_control_chars + "\"") != std::string::npos );
    }

    SECTION("a value that does not fit in the buffer") {
        const std::string too_long(sizeof(kii_buff), 'x');

        CHECK( kii_auth_thing(&kii, too_long.c_str(), password) == KII_ERR_TOO_LARGE_DATA );
        CHECK( kii_auth_thing(&kii, vendor_thing_id, too_long.c_str()) == KII_ERR_TOO_LARGE_DATA );
        CHECK( kii_register_thing(&kii, vendor_thing_id, too_long.c_str(), password) == KII_ERR_TOO_LARGE_DATA );
        CHECK( kii_ti_put_firmware_version(&kii, too_long.c_str()) == KII_ERR_TOO_LARGE_DATA );
        CHECK( sent.empty() );
        // 2.x onboards with an anonymous token, which it asks for first: only
        // the onboarding itself is left unsent.
        responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
        CHECK( kii_ti_onboard(&kii, vendor_thing_id, password, too_long.c_str(), NULL, NULL, NULL) == KII_ERR_TOO_LARGE_DATA );
        CHECK( sent.find("/onboardings") == std::string::npos );
    }

    SECTION("kii_auth_thing") {
        responses.push_back(ok_response("{\"id\":\"th.1\",\"access_token\":\"tok\"}"));

        kii_code_t res = kii_auth_thing(&kii, vendor_thing_id, password);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"username\":\"VENDOR_THING_ID:ab\"") != std::string::npos );
        REQUIRE( sent.find("\"password\":\"" + esc_password + "\"") != std::string::npos );
    }

    SECTION("kii_register_thing") {
        responses.push_back(ok_response("{\"_thingID\":\"th.1\",\"_accessToken\":\"tok\"}"));

        kii_code_t res = kii_register_thing(&kii, vendor_thing_id, "type", password);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"_vendorThingID\":\"ab\"") != std::string::npos );
        REQUIRE( sent.find("\"_password\":\"" + esc_password + "\"") != std::string::npos );
    }

    SECTION("kii_ti_onboard") {
        // An anonymous token is fetched first, then the onboarding request.
        responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
        responses.push_back(ok_response("{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}"));

        kii_code_t res = kii_ti_onboard(
                &kii, vendor_thing_id, password, "type", NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"vendorThingID\":\"ab\"") != std::string::npos );
        REQUIRE( sent.find("\"thingPassword\":\"" + esc_password + "\"") != std::string::npos );
    }

    // thing_properties is a JSON object, embedded as the value of
    // thingProperties. It was escaped as if it were a string's contents, so
    // every quote in it went out as \" and the body was not JSON.
    SECTION("kii_ti_onboard with thing_properties") {
        responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
        responses.push_back(ok_response("{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}"));

        kii_code_t res = kii_ti_onboard(
                &kii, vendor_thing_id, password, "type", NULL, NULL,
                "{\"k\":1,\"s\":\"v\"}");

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"thingProperties\":{\"k\":1,\"s\":\"v\"}") != std::string::npos );

        // The body is sent chunked, in a single chunk at this size: it ends
        // where the terminating zero-length chunk begins.
        size_t start = sent.find("{\"vendorThingID\"");
        size_t end = sent.rfind("\r\n0\r\n\r\n");
        REQUIRE( start != std::string::npos );
        REQUIRE( end != std::string::npos );
        picojson::value v;
        std::string err = picojson::parse(v, sent.substr(start, end - start));
        REQUIRE( err == "" );
        REQUIRE( v.is<picojson::object>() );
        picojson::value props = v.get<picojson::object>()["thingProperties"];
        REQUIRE( props.is<picojson::object>() );
        REQUIRE( props.get<picojson::object>()["k"].get<double>() == 1 );
        REQUIRE( props.get<picojson::object>()["s"].get<std::string>() == "v" );
    }
}

// kii_auth_thing(), kii_register_thing() and kii_ti_onboard() read the response
// straight into kii's author. One they couldn't read whole, a 2xx without the
// thing's ID, or with an ID or a token longer than kii has room for, failed the
// call with the author half replaced, and the requests after it went with an
// ID and a token that belonged to no one login.
TEST_CASE( "A login's response that can't be read whole leaves the author as it was" ) {
    char kii_buff[2048];
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };

    kii_t kii;
    kii_init(&kii);
    kii_set_site(&kii, "api.kii.com");
    kii_set_app_id(&kii, "dummyAppID");
    kii_set_buff(&kii, kii_buff, sizeof(kii_buff));
    kii_set_json_parser_resource(&kii, &resource);

    std::deque<std::string> responses;
    std::string resp;
    size_t resp_pos = 0;
    khct::cb::SockCtx http_ctx;
    http_ctx.on_connect = [&](void*, const char*, unsigned int) {
        if (responses.empty()) {
            return KHC_SOCK_FAIL;
        }
        resp = responses.front();
        responses.pop_front();
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

    const std::string too_long(KII_TOKEN_MAX_SIZE, 'x');
    struct Login {
        const char* name;
        const char* id;
        const char* token;
        std::function<kii_code_t()> call;
        bool anonymous_token_first;
    };
    const Login logins[] = {
        { "kii_auth_thing", "id", "access_token",
            [&kii]() { return kii_auth_thing(&kii, "vid", "pass"); }, false },
        { "kii_register_thing", "_thingID", "_accessToken",
            [&kii]() { return kii_register_thing(&kii, "vid", "type", "pass"); }, false },
        { "kii_ti_onboard", "thingID", "accessToken",
            [&kii]() { return kii_ti_onboard(&kii, "vid", "pass", NULL, NULL, NULL, NULL); }, true },
    };
    for (const Login& login : logins) {
        const std::string id = login.id;
        const std::string token = login.token;
        const std::string bodies[] = {
            "{\"" + token + "\":\"tok.new\"}",
            "{\"" + id + "\":\"th.new\"}",
            "{\"" + id + "\":\"th.new\",\"" + token + "\":\"" + too_long + "\"}",
            "{\"" + id + "\":\"" + too_long + "\",\"" + token + "\":\"tok.new\"}",
            "{\"" + id + "\":\"th.new\",\"" + token + "\":",
        };
        for (const std::string& body : bodies) {
            INFO(login.name << " given " << body.substr(0, 60));
            strcpy(kii._author.author_id, "th.old");
            strcpy(kii._author.access_token, "tok.old");
            responses.clear();
            if (login.anonymous_token_first) {
                responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
            }
            responses.push_back(ok_response(body));
            CHECK( login.call() == KII_ERR_PARSE_JSON );
            CHECK( std::string(kii._author.author_id) == "th.old" );
            CHECK( std::string(kii._author.access_token) == "tok.old" );
        }

        INFO(login.name << " given a response it can read");
        responses.clear();
        if (login.anonymous_token_first) {
            responses.push_back(ok_response("{\"access_token\":\"anon\"}"));
        }
        responses.push_back(ok_response(
                    "{\"" + id + "\":\"th.new\",\"" + token + "\":\"tok.new\"}"));
        CHECK( login.call() == KII_ERR_OK );
        CHECK( std::string(kii._author.author_id) == "th.new" );
        CHECK( std::string(kii._author.access_token) == "tok.new" );
    }
}
