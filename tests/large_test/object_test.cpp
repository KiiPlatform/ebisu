#include <stdio.h>
#include <stdlib.h>
#include <cstring>

#include <kii.h>
#include <kii_json.h>
#include "secure_socket_impl.h"
#include "catch.hpp"
#include "large_test.h"

TEST_CASE("Object Tests")
{
    kii_code_t ret = KII_ERR_FAIL;
    size_t buff_size = 4096;
    char buff[buff_size];
    kii_t kii;
    kiiltest::ssl::SSLData http_ssl_ctx;
    kiiltest::ssl::SSLData mqtt_ssl_ctx;

    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &mqtt_ssl_ctx);

    const char vid[] = "test1";
    const char password[] = "1234";
    ret = kii_thing_authenticate(&kii, vid, password);

    REQUIRE( ret == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    REQUIRE( std::string(kii._author.author_id).length() > 0 );
    REQUIRE( std::string(kii._author.access_token).length() > 0 );

    SECTION("App Scope Object")
    {
        kii_bucket_t bucket;
        bucket.bucket_name = "my_bucket";
        bucket.scope = KII_SCOPE_APP;
        bucket.scope_id = NULL;

        SECTION("POST") {
            const char object[] = "{}";
            char object_id[128];
            object_id[0] = '\0';
            kii_code_t code = kii_object_post(&kii, &bucket, object, NULL, object_id);

            REQUIRE( code == KII_ERR_OK );
            REQUIRE( khc_get_status_code(&kii._khc) == 201 );
            REQUIRE( strlen(object_id) > 0 );
            char* etag = kii_get_etag(&kii);
            REQUIRE( strlen(etag) > 0 );
        }

        SECTION("PUT") {
            std::string id_base("myobj-");
            std::string id = std::to_string(kiiltest::current_time());
            std::string object_id = id_base + id;
            const char object_data[] = "{}";
            kii_code_t code = kii_object_put(&kii, &bucket, object_id.c_str(), object_data, "", NULL);

            REQUIRE( code == KII_ERR_OK );
            REQUIRE( khc_get_status_code(&kii._khc) == 201 );
        }
    }
}