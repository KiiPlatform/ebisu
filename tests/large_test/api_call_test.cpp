#include "catch.hpp"
#include "kii.h"
#include <thread>
#include "large_test.h"
#include "picojson.h"

TEST_CASE("API call tests")
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

    SECTION("API call") {
        // POST object
        char path[128];
        int path_len = snprintf(path, 128, "/api/apps/%s/buckets/test_bucket/objects", kii._app_id);
        REQUIRE(path_len <= 128);
        const char content_type[] = "application/json";

        kii_code_t start_res = kii_api_call_start(&kii, "POST", path, content_type, KII_TRUE);
        REQUIRE (start_res == KII_ERR_OK);

        const char chunk1[] = "{\"long_text\" : \"";
        kii_code_t apend_res = kii_api_call_append_body(&kii, chunk1, strlen(chunk1));
        REQUIRE (apend_res == KII_ERR_OK);

        const char chunk2[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
        apend_res = kii_api_call_append_body(&kii, chunk2, strlen(chunk2));
        REQUIRE (apend_res == KII_ERR_OK);

        const char chunk3[] = "\"}";
        apend_res = kii_api_call_append_body(&kii, chunk3, strlen(chunk3));
        REQUIRE (apend_res == KII_ERR_OK);

        kii_code_t run_res = kii_api_call_run(&kii);
        CHECK (run_res == KII_ERR_OK);
        REQUIRE (kii_get_resp_status(&kii) == 201);

        // Parse response.
        picojson::value v;
        auto err_str = picojson::parse(v, buff);
        REQUIRE ( err_str.empty() );
        REQUIRE ( v.is<picojson::object>() );
        picojson::object resp = v.get<picojson::object>();
        auto object_id = resp.at("objectID");
        REQUIRE ( object_id.is<std::string>() );
        REQUIRE ( object_id.get<std::string>().length() > 0 );

        SECTION("API call without body") {
            // Get object.
            const char* obj_id = object_id.get<std::string>().c_str();
            int path_len = snprintf(path, 128, "/api/apps/%s/buckets/test_bucket/objects/%s", kii._app_id, obj_id);
            REQUIRE(path_len <= 128);

            start_res = kii_api_call_start(&kii, "GET", path, NULL, KII_FALSE);
            REQUIRE (start_res == KII_ERR_OK);

            run_res = kii_api_call_run(&kii);
            CHECK (run_res == KII_ERR_OK);
            REQUIRE (kii_get_resp_status(&kii) == 200);

            // Parse response.
            picojson::value v;
            auto err_str = picojson::parse(v, buff);
            REQUIRE ( err_str.empty() );
            REQUIRE ( v.is<picojson::object>() );
            picojson::object resp_get = v.get<picojson::object>();
            auto long_text = resp_get.at("long_text");
            REQUIRE ( long_text.is<std::string>() );
            REQUIRE ( long_text.get<std::string>() == std::string(chunk2));
        }
        SECTION("API call with header") {
            // PUT object with If-None-Match
            const char* obj_id = object_id.get<std::string>().c_str();
            int path_len = snprintf(path, 128, "/api/apps/%s/buckets/test_bucket/objects/%s", kii._app_id, obj_id);
            REQUIRE(path_len <= 128);
            start_res = kii_api_call_start(&kii, "PUT", path, "application/json", KII_TRUE);
            REQUIRE (start_res == KII_ERR_OK);

            apend_res = kii_api_call_append_body(&kii, "{}", 2);
            REQUIRE (apend_res == KII_ERR_OK);

            kii_code_t header_append_res = kii_api_call_append_header(&kii, "If-None-Match", "*");
            REQUIRE (header_append_res == KII_ERR_OK);

            run_res = kii_api_call_run(&kii);
            CHECK (run_res == KII_ERR_OK);
            // If-None-Match: * will fail since there's a object created.
            REQUIRE (kii_get_resp_status(&kii) == 409);
        }
    }

}