#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <ctime>
#include <sstream>
#include <functional>

#include <kii.h>
#include <jkii.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"
#include "picojson.h"

TEST_CASE("TI Tests")
{
    // To Avoid 429 Too Many Requests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size_t buff_size = 4096;
    char buff[buff_size];
    kii_t kii;
    ebisu::ltest::ssl::SSLData http_ssl_ctx;
    ebisu::ltest::ssl::SSLData mqtt_ssl_ctx;

    jkii_token_t tokens[256];
    jkii_resource_t resource = {tokens, 256};

    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &mqtt_ssl_ctx, &resource);

    SECTION("Onboard minimum parameters")
    {
        const char vid[] = "test1";
        const char password[] = "1234";
        kii_code_t res = kii_ti_onboard(&kii, vid, password, NULL, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );
    }

    SECTION("Get firmware version")
    {
        std::ostringstream oss;
        oss << "LTest-" << std::time(NULL);
        std::string vid = oss.str();
        const char password[] = "1234";
        const char firmware[] = "0.0.2";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, NULL, firmware, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        kii_ti_firmware_version_t version;
        memset(&version, 0x00, sizeof(kii_ti_firmware_version_t));
        res = kii_ti_get_firmware_version(&kii, &version);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(version.firmware_version) == std::string(firmware) );
    }

    SECTION("Put firmware version")
    {
        std::ostringstream oss;
        oss << "LTest-" << std::time(NULL);
        std::string vid = oss.str();
        const char password[] = "1234";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, NULL, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        kii_ti_firmware_version_t version;
        memset(&version, 0x00, sizeof(kii_ti_firmware_version_t));
        res = kii_ti_get_firmware_version(&kii, &version);

        REQUIRE( res == KII_ERR_RESP_STATUS );
        REQUIRE( khc_get_status_code(&kii._khc) == 404 );
        REQUIRE( std::string(version.firmware_version).length() == 0 );

        const char firmware[] = "0.0.3";
        res = kii_ti_put_firmware_version(&kii, firmware);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );

        res = kii_ti_get_firmware_version(&kii, &version);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(version.firmware_version) == std::string(firmware) );
    }

    SECTION("Put state")
    {
        const char vid[] = "test1";
        const char password[] = "1234";
        kii_code_t res = kii_ti_onboard(&kii, vid, password, NULL, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        std::string body = "{\"dummyKey\":\"dummyvalue\"}";
        std::istringstream iss(body);
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [=, &iss](char *buffer, size_t size, void *userdata)
            {
                return iss.read(buffer, size).gcount();
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_put_state(&kii, kiiltest::read_cb, &ctx, "application/json", NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    }

    SECTION("Put bulk states")
    {
        std::ostringstream oss;
        oss << "ltest_vid_" << std::time(NULL);
        std::string vid = oss.str();
        std::time_t t = std::time(NULL);
        const char password[] = "1234";
        const char thing_type[] = "ltest_thing_type";
        const char firmware_version[] = "ltest_firmware_version";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, thing_type, firmware_version, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        std::string body = "{\"states\":[{\"generatedAt\":" + std::to_string(t) + "000,\"payload\":{\"ltest_trait_alias\":{\"string_field\":\"value\"}}}]}";
        std::istringstream iss(body);
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [=, &iss](char *buffer, size_t size, void *userdata)
            {
                return iss.read(buffer, size).gcount();
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_put_bulk_states(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    }

    SECTION("Patch state")
    {
        std::ostringstream oss;
        oss << "ltest_vid_" << std::time(NULL);
        std::string vid = oss.str();
        const char password[] = "1234";
        const char thing_type[] = "ltest_thing_type";
        const char firmware_version[] = "ltest_firmware_version";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, thing_type, firmware_version, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        std::string body = "{\"ltest_trait_alias\":{\"string_field\":\"value\"}}";
        std::istringstream iss(body);
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [=, &iss](char *buffer, size_t size, void *userdata)
            {
                return iss.read(buffer, size).gcount();
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_patch_state(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    }

    SECTION("Patch bulk states")
    {
        std::ostringstream oss;
        oss << "ltest_vid_" << std::time(NULL);
        std::string vid = oss.str();
        std::time_t t = std::time(NULL);

        const char password[] = "1234";
        const char thing_type[] = "ltest_thing_type";
        const char firmware_version[] = "ltest_firmware_version";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, thing_type, firmware_version, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        std::string body = "{\"states\":[{\"generatedAt\":" + std::to_string(t) + "000,\"payload\":{\"ltest_trait_alias\":{\"string_field\":\"value\"}}}]}";
        std::istringstream iss(body);
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [=, &iss](char *buffer, size_t size, void *userdata)
            {
                return iss.read(buffer, size).gcount();
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_patch_bulk_states(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    }
}

TEST_CASE("Empty request payloads", "[empty-request]") {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size_t buff_size = 4096;
    char buff[buff_size];
    kii_t kii;
    ebisu::ltest::ssl::SSLData http_ssl_ctx;
    ebisu::ltest::ssl::SSLData mqtt_ssl_ctx;

    jkii_token_t tokens[256];
    jkii_resource_t resource = {tokens, 256};

    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &mqtt_ssl_ctx, &resource);

    std::ostringstream oss;
    oss << "ltest_vid_" << std::time(NULL);
    std::string vid = oss.str();
    const char password[] = "1234";
    const char thing_type[] = "ltest_thing_type";
    const char firmware_version[] = "ltest_firmware_version";
    kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, thing_type, firmware_version, NULL, NULL);

    REQUIRE( res == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    REQUIRE( std::string(kii._author.author_id).length() > 0 );
    REQUIRE( std::string(kii._author.access_token).length() > 0 );

    SECTION("Put bulk states with empty callback")
    {

        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [] (char *buffer, size_t size, void *userdata)
            {
                return 0;
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_put_bulk_states(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Put bulk states with no callback")
    {
        res = kii_ti_put_bulk_states(&kii, NULL, NULL, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Patch bulk states with empty callback")
    {

        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [](char *buffer, size_t size, void *userdata)
            {
                return 0;
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_patch_bulk_states(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Patch bulk states with null callback")
    {
        res = kii_ti_patch_bulk_states(&kii, NULL, NULL, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Patch state with empty callback")
    {
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [](char *buffer, size_t size, void *userdata)
            {
                return 0;
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_patch_state(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Patch state with null callback")
    {
        res = kii_ti_patch_state(&kii, NULL, NULL, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Put state with empty callback")
    {
        std::function<size_t(char *buffer, size_t size, void *userdata)>
            on_read = [](char *buffer, size_t size, void *userdata)
            {
                return 0;
            };
        kiiltest::RWFunc ctx;
        ctx.on_read = on_read;
        res = kii_ti_put_state(&kii, kiiltest::read_cb, &ctx, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }

    SECTION("Put state with null callback")
    {
        res = kii_ti_put_state(&kii, NULL, NULL, NULL, NULL, NULL);
        kiiltest::check_error_response(kii, buff, res, 400, "EMPTY_BODY");
    }
}
