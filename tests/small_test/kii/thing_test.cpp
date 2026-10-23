#include "catch.hpp"
#include "kii.h"
#include "picojson.h"
#include "test_callbacks.h"

#include <deque>
#include <string>
#include <string.h>
#include <utility>

static std::string ok_response(const std::string& body) {
    return "HTTP/1.1 200 OK\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
}

static kii_author_t author_of(kii_t* kii) {
    kii_author_t author;
    kii_get_author(kii, &author);
    return author;
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

    // None until one is obtained.
    REQUIRE( std::string(author_of(&kii).author_id) == "" );
    REQUIRE( std::string(author_of(&kii).access_token) == "" );

    const char vendor_thing_id[] = "ab";
    // Far beyond twice the ID, and with characters JSON needs escaped.
    const char password[] = "a/password\"much-longer-than-the-vendor-thing-id";
    const std::string esc_password = "a\\/password\\\"much-longer-than-the-vendor-thing-id";

    SECTION("kii_auth_thing") {
        responses.push_back(ok_response("{\"id\":\"th.1\",\"access_token\":\"tok\"}"));

        kii_code_t res = kii_auth_thing(&kii, vendor_thing_id, password);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"username\":\"VENDOR_THING_ID:ab\"") != std::string::npos );
        REQUIRE( sent.find("\"password\":\"" + esc_password + "\"") != std::string::npos );
        REQUIRE( std::string(author_of(&kii).author_id) == "th.1" );
        REQUIRE( std::string(author_of(&kii).access_token) == "tok" );
    }

    SECTION("kii_register_thing") {
        responses.push_back(ok_response("{\"_thingID\":\"th.1\",\"_accessToken\":\"tok\"}"));

        kii_code_t res = kii_register_thing(&kii, vendor_thing_id, "type", password);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("\"_vendorThingID\":\"ab\"") != std::string::npos );
        REQUIRE( sent.find("\"_password\":\"" + esc_password + "\"") != std::string::npos );
        REQUIRE( std::string(author_of(&kii).author_id) == "th.1" );
        REQUIRE( std::string(author_of(&kii).access_token) == "tok" );
    }

    SECTION("kii_ti_onboard") {
        // The onboarding request is the only one: the app's ID authorizes it.
        responses.push_back(ok_response("{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}"));

        kii_code_t res = kii_ti_onboard(
                &kii, vendor_thing_id, password, "type", NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( sent.find("POST https://api.kii.com/thing-if/apps/dummyAppID/onboardings HTTP/1.1\r\n") == 0 );
        REQUIRE( sent.find("\r\nAuthorization: Basic ZHVtbXlBcHBJRDpkdW1teQ==\r\n") != std::string::npos );
        REQUIRE( sent.find("\"vendorThingID\":\"ab\"") != std::string::npos );
        REQUIRE( sent.find("\"thingPassword\":\"" + esc_password + "\"") != std::string::npos );
        REQUIRE( std::string(author_of(&kii).author_id) == "th.1" );
        REQUIRE( std::string(author_of(&kii).access_token) == "tok" );
    }

    // The app's ID and a placeholder key, base64 encoded, whatever the
    // credentials' length: one, two or no bytes left over a multiple of three,
    // and the longest app ID there is room for.
    SECTION("kii_ti_onboard is authorized with the app's ID") {
        // 127 x's: 42 times "xxx", then "x:dummy".
        std::string longest_basic;
        for (int i = 0; i < 42; ++i) {
            longest_basic += "eHh4";
        }
        longest_basic += "eDpkdW1teQ==";
        const std::pair<std::string, std::string> apps[] = {
            { "a", "YTpkdW1teQ==" },
            { "ab", "YWI6ZHVtbXk=" },
            { "abc", "YWJjOmR1bW15" },
            { std::string(KII_APP_ID_MAX_SIZE - 1, 'x'), longest_basic },
        };
        for (const auto& app : apps) {
            kii_set_app_id(&kii, app.first.c_str());
            sent.clear();
            responses.push_back(ok_response("{\"accessToken\":\"tok\",\"thingID\":\"th.1\"}"));

            kii_code_t res = kii_ti_onboard(
                    &kii, vendor_thing_id, password, "type", NULL, NULL, NULL);

            REQUIRE( res == KII_ERR_OK );
            CHECK( sent.find("\r\nAuthorization: Basic " + app.second + "\r\n") != std::string::npos );
        }
    }

    // thing_properties is a JSON object, embedded as the value of
    // thingProperties. It was escaped as if it were a string's contents, so
    // every quote in it went out as \" and the body was not JSON.
    SECTION("kii_ti_onboard with thing_properties") {
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
