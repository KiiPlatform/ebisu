#include "catch.hpp"
#include <thread>
#include <cstring>
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
        kii_object_id_t obj_id;
        kii_code_t post_res = kii_post_object(&kii, &bucket, object, NULL, &obj_id);
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
        int status = khc_get_status_code(&kii._khc);
        // TODO: Make admin operation tools in test and create new topic by the tool.
        REQUIRE((status == 204 || status == 409));

        kii_code_t del_res = kii_unsubscribe_topic(&kii, &topic);
        CHECK(del_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 204);
    }
    SECTION("Thing scope topic") {
        kii_topic_t topic;
        topic.scope = KII_SCOPE_THING;
        topic.scope_id = kii._author.author_id;
        std::string name_base = std::string("topic-");
        std::string id = std::to_string(kiiltest::current_time());
        const char* topic_name = (name_base + id).c_str();
        topic.topic_name = topic_name;

        kii_code_t put_res = kii_put_topic(&kii, &topic);
        CHECK(put_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 204);

        // Putting same topic results 409
        kii_code_t put_res2 = kii_put_topic(&kii, &topic);
        CHECK(put_res2 == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 409);

        SECTION("Delete thing scope topic") {
            // To Avoid 429 Too Many Requests
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            kii_code_t del_res = kii_delete_topic(&kii, &topic);
            CHECK(del_res == KII_ERR_OK);
            REQUIRE(khc_get_status_code(&kii._khc) == 204);

            // Deleting same topic results 404.
            kii_code_t del_res2 = kii_delete_topic(&kii, &topic);
            CHECK(del_res2 == KII_ERR_RESP_STATUS);
            REQUIRE(khc_get_status_code(&kii._khc) == 404);
        }
    }
    SECTION("Install push") {
        kii_installation_id_t ins_id;
        kii_code_t ins_res = kii_install_push(&kii, KII_TRUE, &ins_id);
        CHECK(ins_res == KII_ERR_OK);
        REQUIRE(khc_get_status_code(&kii._khc) == 201);
        REQUIRE(strlen(ins_id.id) > 0);

        SECTION("Get MQTT endpoint") {
            kii_mqtt_endpoint_t endpoint;
            memset(&endpoint, '\0', sizeof(kii_mqtt_endpoint_t));

            int status = 0;
            kii_code_t get_ep_res = KII_ERR_FAIL;
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                get_ep_res = kii_get_mqtt_endpoint(&kii, ins_id.id, &endpoint);
                status = khc_get_status_code(&kii._khc);
            } while (status == 503);

            CHECK(get_ep_res == KII_ERR_OK);
            REQUIRE(khc_get_status_code(&kii._khc) == 200);
            REQUIRE(strlen(endpoint.host) > 0);
            REQUIRE(strlen(endpoint.password) > 0);
            REQUIRE(strlen(endpoint.topic) > 0);
            REQUIRE(strlen(endpoint.username) > 0);
            REQUIRE(endpoint.port_ssl == 8883);
            REQUIRE(endpoint.port_tcp == 1883);
        }
    }
}