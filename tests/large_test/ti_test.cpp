#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <sstream>
#include <functional>

#include <kii.h>
#include <kii_json.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"
#include "picojson.h"

#define STATE_BODY "{\"dummyKey\":\"dummyvalue\"}"

static size_t readCB(char* buffer, size_t size, size_t count, void *userdata)
{
    std::istringstream iss = std::istringstream(std::string(STATE_BODY));
    return iss.read(buffer, size * count).gcount();
}


TEST_CASE("TI Tests")
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

        std::string body(STATE_BODY);
        res = kii_ti_put_state(&kii, body.length(), readCB, NULL);

        REQUIRE( res == KII_ERR_OK );
        REQUIRE( khc_get_status_code(&kii._khc) == 204 );
    }
}
