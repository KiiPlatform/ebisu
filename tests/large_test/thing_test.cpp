#define CATCH_CONFIG_MAIN
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>

#include <kii.h>
#include <kii_json.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"

TEST_CASE("Thing tests")
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

    SECTION("Thing auth") {
        kii_code_t ret = KII_ERR_FAIL;
        const char vid[] = "test1";
        const char password[] = "1234";
        ret = kii_auth_thing(&kii, vid, password);

        REQUIRE( ret == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );
    }
    SECTION("Thing register") {
        kii_code_t ret = KII_ERR_FAIL;
        std::string vid_base("thing2-");
        std::string id = std::to_string(kiiltest::current_time());
        std::string vid = vid_base + id;
        const char thing_type[] = "ltest-thing";
        const char password[] = "1234";
        ret = kii_register_thing(&kii, vid.c_str(), thing_type, password);

        REQUIRE( ret == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 201 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );
    }
}