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

TEST_CASE("Object Tests")
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

    SECTION("App Scope Object")
    {
        kii_bucket_t bucket;
        bucket.bucket_name = "my_bucket";
        bucket.scope = KII_SCOPE_APP;
        bucket.scope_id = NULL;

        SECTION("POST") {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            const char object[] = "{}";
            kii_object_id_t obj_id;
            kii_code_t post_res = kii_post_object(&kii, &bucket, object, NULL, &obj_id);

            REQUIRE( post_res == KII_ERR_OK );
            REQUIRE( khc_get_status_code(&kii._khc) == 201 );
            REQUIRE( strlen(obj_id.id) > 0 );
            const char* etag = kii_get_etag(&kii);
            size_t etag_len = strlen(etag);
            REQUIRE( etag_len == 3 );

            SECTION("PUT with Etag") {
                char etag_copy[etag_len+1];
                memcpy(etag_copy, etag, etag_len);
                etag_copy[etag_len] = '\0';
                kii_code_t put_res = kii_put_object(&kii, &bucket, obj_id.id, object, NULL, etag_copy);
                REQUIRE( put_res == KII_ERR_OK );
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );
                // Now etag_copy should be obsoleted.
                put_res = kii_put_object(&kii, &bucket, obj_id.id, object, NULL, etag_copy);
                REQUIRE( put_res == KII_ERR_RESP_STATUS );
                REQUIRE( khc_get_status_code(&kii._khc) == 409 );
            }
            SECTION("PATCH") {
                const char patch_data[] = "{\"patch\":1}";
                kii_code_t patch_res = kii_patch_object(&kii, &bucket, obj_id.id, patch_data, NULL);
                REQUIRE( patch_res == KII_ERR_OK );
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );

                const char* p_etag = kii_get_etag(&kii);
                size_t p_etag_len = strlen(etag);
                char p_etag_copy[p_etag_len+1];
                memcpy(p_etag_copy, p_etag, p_etag_len);
                p_etag_copy[p_etag_len] = '\0';

                patch_res = kii_patch_object(&kii, &bucket, obj_id.id, patch_data, p_etag_copy);
                REQUIRE( patch_res == KII_ERR_OK );
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );
                // Now p_etag_copy is not valid.
                patch_res = kii_patch_object(&kii, &bucket, obj_id.id, patch_data, p_etag_copy);
                REQUIRE( patch_res == KII_ERR_RESP_STATUS );
                REQUIRE( khc_get_status_code(&kii._khc) == 409 );
            }
            SECTION("GET") {
                kii_code_t get_res = kii_get_object(&kii, &bucket, obj_id.id);
                REQUIRE( get_res == KII_ERR_OK );
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );

                // Parse response.
                picojson::value v;
                auto err_str = picojson::parse(v, buff);
                REQUIRE ( err_str.empty() );
                REQUIRE ( v.is<picojson::object>() );
                picojson::object obj = v.get<picojson::object>();
                auto obj_id_ = obj.at("_id");
                REQUIRE ( obj_id_.is<std::string>() );
                REQUIRE ( obj_id_.get<std::string>() == std::string(obj_id.id) );
            }
            SECTION("DELETE") {
                kii_code_t delete_res = kii_delete_object(&kii, &bucket, obj_id.id);
                REQUIRE( delete_res == KII_ERR_OK );
                REQUIRE( khc_get_status_code(&kii._khc) == 204 );
                // Now get should return 404.
                kii_code_t get_res = kii_get_object(&kii, &bucket, obj_id.id);
                REQUIRE( get_res == KII_ERR_RESP_STATUS );
                REQUIRE( khc_get_status_code(&kii._khc) == 404 );
            }
            SECTION("Upload Download Body") {
                std::string body(
                    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
                );
                std::istringstream iss(body);
                std::function<size_t(char *buffer, size_t size, size_t count, void *userdata)>
                    on_read = [=, &iss](char *buffer, size_t size, size_t count, void *userdata)
                {
                    return iss.read(buffer, size * count).gcount();
                };
                kiiltest::BodyFunc ctx;
                ctx.on_read = on_read;
                kii_code_t upload_res = kii_upload_object_body(&kii, &bucket, obj_id.id, "text/plain", body.length(), kiiltest::read_cb, &ctx);
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );
                REQUIRE( upload_res == KII_ERR_OK );

                // Download uploaded body
                std::ostringstream oss;
                std::function<size_t(char *buffer, size_t size, size_t count, void *userdata)>
                    on_write = [=, &oss](char *buffer, size_t size, size_t count, void *userdata)
                {
                    oss.write(buffer, size * count);
                    return size * count;
                };
                ctx.on_write = on_write;
                kii_code_t download_res = kii_download_object_body(&kii, &bucket, obj_id.id, kiiltest::write_cb, &ctx);
                REQUIRE( khc_get_status_code(&kii._khc) == 200 );
                REQUIRE( download_res == KII_ERR_OK );
                REQUIRE ( oss.str() == body );
            }
        }

        SECTION("PUT") {
            std::string id_base("myobj-");
            std::string id = std::to_string(kiiltest::current_time());
            std::string object_id = id_base + id;
            const char object_data[] = "{}";
            kii_code_t put_res = kii_put_object(&kii, &bucket, object_id.c_str(), object_data, "", NULL);

            REQUIRE( put_res == KII_ERR_OK );
            REQUIRE( khc_get_status_code(&kii._khc) == 201 );
        }
    }
}