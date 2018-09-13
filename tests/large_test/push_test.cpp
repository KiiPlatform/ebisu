#include "catch.hpp"
#include <thread>
#include <kii.h>
#include "large_test.h"

TEST_CASE("Push Tests")
{
    // To Avoid 429 Too Many Requests
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    size_t buff_size = 4096;
    char buff[buff_size];
    kii_t kii;
    kiiltest::ssl::SSLData http_ssl_ctx;
    kiiltest::ssl::SSLData mqtt_ssl_ctx;

    kiiltest::init(&kii, buff, buff_size, &http_ssl_ctx, &mqtt_ssl_ctx);

    const char vid[] = "test1";
    const char password[] = "1234";
    kii_code_t auth_res = kii_thing_auth(&kii, vid, password);

    REQUIRE( auth_res == KII_ERR_OK );
    REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    REQUIRE( std::string(kii._author.author_id).length() > 0 );
    REQUIRE( std::string(kii._author.access_token).length() > 0 );

    SECTION("Subscribe to app scope bucket") {
        kii_bucket_t bucket;
        std::string bucket_name_base = std::string("bucket-");
        std::string id = std::to_string(kiiltest::current_time());
        const char* bucket_name = (bucket_name_base + id).c_str();
        bucket.bucket_name = bucket_name;
        bucket.scope = KII_SCOPE_APP;
        bucket.scope_id = NULL;

        // Post object to create bucket.
        const char object[] = "{}";
        char object_id[128];
        object_id[0] = '\0';
        kii_code_t post_res = kii_object_post(&kii, &bucket, object, NULL, object_id);
        CHECK(post_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 201);

        kii_code_t sub_res = kii_subscribe_bucket(&kii, &bucket);
        CHECK(sub_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 204);

        kii_code_t sub_res2 = kii_subscribe_bucket(&kii, &bucket);
        CHECK(sub_res2 == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 409);

        kii_code_t unsub_res = kii_unsubscribe_bucket(&kii, &bucket);
        CHECK(unsub_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 204);

        kii_code_t unsub_res2 = kii_unsubscribe_bucket(&kii, &bucket);
        CHECK(unsub_res2 == KII_ERR_RESP_STATUS);
        REQUIRE(khc_get_status_code(&kii._khc) == 404);
    }
    SECTION("Subscribe to app scope topic") {
        kii_topic_t topic;
        topic.scope = KII_SCOPE_APP;
        topic.scope_id = "";
        topic.topic_name = "test_topic";

        kii_code_t sub_res = kii_subscribe_topic(&kii, &topic);
        CHECK(sub_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 204);

    }
}