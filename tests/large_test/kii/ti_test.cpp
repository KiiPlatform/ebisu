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

namespace {

// The thing's latest state, read with its own token, once it has the value
// given at path, a "/"-separated list of members: a state just uploaded may
// take a moment to be read back. Null if it never does.
picojson::value read_state(kii_t& kii, const std::string& path, const std::string& value)
{
    const std::string resource = "/thing-if/apps/" + std::string(APP_ID) + "/targets/thing:" +
        kii._author.author_id + "/states";
    for (int i = 0; i < 10; ++i) {
        if (i > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        REQUIRE( kii_api_call_start(&kii, "GET", resource.c_str(), NULL, KII_TRUE) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&kii) == 200 );
        picojson::value state;
        REQUIRE( picojson::parse(state, std::string(kii._rw_buff, kii_get_resp_body_length(&kii))).empty() );
        picojson::value v = state;
        std::istringstream members(path);
        std::string member;
        while (std::getline(members, member, '/') && v.is<picojson::object>()) {
            v = v.contains(member) ? v.get(member) : picojson::value();
        }
        if (v.is<std::string>() && v.get<std::string>() == value) {
            return state;
        }
    }
    return picojson::value();
}

}

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

    SECTION("Onboard with every parameter, read back")
    {
        const std::string vid = "ltest_vid_" + std::to_string(kiiltest::current_time());
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), "1234", "ltest_thing_type",
                "ltest_firmware_version", "STANDALONE",
                "{\"_vendor\":\"ltest-vendor\",\"_productName\":\"ltest-product\"}");
        REQUIRE( res == KII_ERR_OK );

        // The thing as Kii Cloud registered it, read with its own token.
        const std::string path = "/api/apps/" + std::string(APP_ID) + "/things/" + kii._author.author_id;
        REQUIRE( kii_api_call_start(&kii, "GET", path.c_str(), NULL, KII_TRUE) == KII_ERR_OK );
        REQUIRE( kii_api_call_run(&kii) == KII_ERR_OK );
        REQUIRE( kii_get_resp_status(&kii) == 200 );
        picojson::value thing;
        REQUIRE( picojson::parse(thing, std::string(kii._rw_buff, kii_get_resp_body_length(&kii))).empty() );
        auto field = [&thing](const char* name) {
            return thing.contains(name) && thing.get(name).is<std::string>() ?
                thing.get(name).get<std::string>() : std::string();
        };
        CHECK( field("_vendorThingID") == vid );
        CHECK( field("_thingType") == "ltest_thing_type" );
        CHECK( field("_firmwareVersion") == "ltest_firmware_version" );
        CHECK( field("_layoutPosition") == "STANDALONE" );
        CHECK( field("_vendor") == "ltest-vendor" );
        CHECK( field("_productName") == "ltest-product" );
    }

    SECTION("Onboarding an existing thing with another password is refused")
    {
        const std::string vid = "ltest_vid_" + std::to_string(kiiltest::current_time());
        REQUIRE( kii_ti_onboard(&kii, vid.c_str(), "1234", NULL, NULL, NULL, NULL) == KII_ERR_OK );

        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), "4321", NULL, NULL, NULL, NULL);
        REQUIRE( res == KII_ERR_RESP_STATUS );
        int status = khc_get_status_code(&kii._khc);
        CHECK( 400 <= status );
        CHECK( status < 500 );

        // The right one still onboards it.
        REQUIRE( kii_ti_onboard(&kii, vid.c_str(), "1234", NULL, NULL, NULL, NULL) == KII_ERR_OK );
    }

    SECTION("Get firmware version")
    {
        std::ostringstream oss;
        oss << "LTest-" << kiiltest::current_time();
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
        oss << "LTest-" << kiiltest::current_time();
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
        // A thing of the run's own: the state is read back, and the other CI
        // cell uploads one at the same time.
        const std::string vid = "ltest_vid_" + std::to_string(kiiltest::current_time());
        const char password[] = "1234";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, NULL, NULL, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        const std::string value = "value-" + std::to_string(kiiltest::current_time());
        std::string body = "{\"dummyKey\":\"" + value + "\"}";
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
        CHECK_FALSE( read_state(kii, "dummyKey", value).is<picojson::null>() );
    }

    SECTION("Put bulk states")
    {
        std::ostringstream oss;
        oss << "ltest_vid_" << kiiltest::current_time();
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
        oss << "ltest_vid_" << kiiltest::current_time();
        std::string vid = oss.str();
        const char password[] = "1234";
        const char thing_type[] = "ltest_thing_type";
        const char firmware_version[] = "ltest_firmware_version";
        kii_code_t res = kii_ti_onboard(&kii, vid.c_str(), password, thing_type, firmware_version, NULL, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 200 );
        REQUIRE( std::string(kii._author.author_id).length() > 0 );
        REQUIRE( std::string(kii._author.access_token).length() > 0 );

        const std::string value = "value-" + std::to_string(kiiltest::current_time());
        std::string body = "{\"ltest_trait_alias\":{\"string_field\":\"" + value + "\"}}";
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
        CHECK_FALSE( read_state(kii, "ltest_trait_alias/string_field", value).is<picojson::null>() );
    }

    SECTION("Patch bulk states")
    {
        std::ostringstream oss;
        oss << "ltest_vid_" << kiiltest::current_time();
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
    oss << "ltest_vid_" << kiiltest::current_time();
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
