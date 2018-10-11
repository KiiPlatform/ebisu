#include "catch.hpp"
#include "kii.h"
#include <thread>
#include "large_test.h"
#include "picojson.h"

TEST_CASE("Server code tests")
{
    // To Avoid 429 Too Many Requests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size_t buff_size = 4096;
    char buff[buff_size];
    kii_t kii;
    kiiltest::ssl::SSLData http_ssl_ctx;
    kiiltest::ssl::SSLData mqtt_ssl_ctx;

    kii_json_token_t tokens[256];
    kii_json_resource_t resource = {tokens, 256};

    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &mqtt_ssl_ctx, &resource);

    const char vid[] = "test1";
    const char password[] = "1234";
    kii_code_t auth_res = kii_auth_thing(&kii, vid, password);

    REQUIRE( auth_res == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    REQUIRE( std::string(kii._author.author_id).length() > 0 );
    REQUIRE( std::string(kii._author.access_token).length() > 0 );

    SECTION("Execute server code") {
        // FIXME: Create echo endpoint by test tools beforehand.
        kii_code_t exec_res = kii_execute_server_code(
            &kii, "echo", "{\"message\":\"hello!\"}");
        REQUIRE( exec_res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );

        // Parse response.
        picojson::value v;
        auto err_str = picojson::parse(v, buff);
        REQUIRE ( err_str.empty() );
        REQUIRE ( v.is<picojson::object>() );
        picojson::object resp = v.get<picojson::object>();
        auto retval = resp.at("returnedValue");
        REQUIRE ( retval.is<std::string>() );
        REQUIRE ( retval.get<std::string>() == std::string("hello!") );
    }

}