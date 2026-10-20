#include "catch.hpp"
#include "kii.h"
#include "test_callbacks.h"

#include <deque>
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
TEST_CASE( "Thing password longer than the vendor thing ID is sent whole" ) {
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
}
