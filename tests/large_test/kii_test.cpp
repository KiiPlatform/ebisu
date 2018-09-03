#define CATCH_CONFIG_MAIN
#include <stdio.h>
#include <stdlib.h>

#include <kii.h>
#include <kii_json.h>
#include "secure_socket_impl.h"
#include "catch.hpp"

static char THING_ID[] = "th.53ae324be5a0-26f8-4e11-a13c-03da6fb2";
static char ACCESS_TOKEN[] = "ablTGrnsE20rSRBFKPnJkWyTaeqQ50msqUizvR_61hU";
static char BUCKET[] = "myBucket";
static char TOPIC[] = "myTopic";
static char BODY[] = "Stop the world!";
static char CONTENT_TYPE[] = "text/plain";
static const char DEFAULT_SITE[] = "api-jp.kii.com";
// APP Owner: satoshi.kumano@kii.com
static const char APP_ID[] = "b6t9ai81zb3s";
static const char APP_KEY[] = "dummy";

static void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        void* http_ssl_ctx,
        void* mqtt_ssl_ctx)
{
    kii_init(kii, DEFAULT_SITE, APP_ID, APP_KEY);

    kii_set_buff(kii, buffer, buffer_size);

    kii_set_http_cb_sock_connect(kii, khct::ssl::cb_connect, http_ssl_ctx);
    kii_set_http_cb_sock_send(kii, khct::ssl::cb_send, http_ssl_ctx);
    kii_set_http_cb_sock_recv(kii, khct::ssl::cb_recv, http_ssl_ctx);
    kii_set_http_cb_sock_close(kii, khct::ssl::cb_close, http_ssl_ctx);

    kii_set_mqtt_cb_sock_connect(kii, khct::ssl::cb_connect, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_send(kii, khct::ssl::cb_send, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_recv(kii, khct::ssl::cb_recv, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_close(kii, khct::ssl::cb_close, mqtt_ssl_ctx);

    strcpy(kii->_author.author_id, THING_ID);
    strcpy(kii->_author.access_token, ACCESS_TOKEN);
}

static void initBucket(kii_bucket_t* bucket)
{
    bucket->scope = KII_SCOPE_THING;
    bucket->scope_id = THING_ID;
    bucket->bucket_name = BUCKET;
}

TEST_CASE("Thing auth")
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    khct::ssl::SSLData http_ssl_ctx;
    khct::ssl::SSLData mqtt_ssl_ctx;

    init(&kii, buffer, 4096, &http_ssl_ctx, &mqtt_ssl_ctx);

    strcpy(kii._author.author_id, "");
    strcpy(kii._author.access_token, "");

    const char vid[] = "test1";
    const char password[] = "1234";
    ret = kii_thing_authenticate(&kii, vid, password);

    REQUIRE( ret == 0);
    REQUIRE( khc_get_status_code(&kii._khc) == 200 );
    REQUIRE( std::string(kii._author.author_id) != "" );
    REQUIRE( std::string(kii._author.access_token) != "" );
}

// TEST(kiiTest, register)
// {
//     int ret = -1;
//     char buffer[4096];
//     char vendorId[1024];
//     kii_t kii;

//     sprintf(vendorId, "%d", getpid());

//     init(&kii, buffer, 4096);

//     strcpy(kii.kii_core.author.author_id, "");
//     strcpy(kii.kii_core.author.access_token, "");
//     kii.kii_core.response_code = 0;
//     ret = kii_thing_register(&kii, vendorId, "my_type", "1234");

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STRNE("", kii.kii_core.author.author_id);
//     ASSERT_STRNE("", kii.kii_core.author.access_token);
// }

// TEST(kiiTest, object)
// {
//     int ret = -1;
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);

//     kii.kii_core.response_code = 0;
//     memset(objectId, 0x00, KII_OBJECTID_SIZE + 1);
//     ret = kii_object_create(&kii, &bucket, "{}", NULL, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_NE(0, strlen(objectId));
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_get(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_TRUE(NULL != kii.kii_core.response_body);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, objectWithID)
// {
//     int ret = -1;
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);
//     strcpy(objectId, "my_object");

//     kii_object_delete(&kii, &bucket, objectId);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_get(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
//     ASSERT_TRUE(NULL != kii.kii_core.response_body);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, objectBodyOnce)
// {
//     int ret = -1;
//     char* body = BODY;
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;
//     unsigned int body_len;
//     unsigned int out_len = 0;

//     body_len = strlen(body);

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);
//     strcpy(objectId, "my_object");

//     kii.kii_core.response_code = 0;
//     ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_upload_body_at_once(&kii, &bucket, objectId, CONTENT_TYPE,
//             body, body_len);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_download_body_at_once(&kii, &bucket, objectId, &out_len);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(body_len, out_len);
//     ASSERT_STREQ(body, kii.kii_core.response_body);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, objectBodyMulti)
// {
//     int ret = -1;
//     char* body = BODY;
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     char uploadId[KII_UPLOADID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;
//     kii_chunk_data_t chunk;
//     unsigned int body_len;
//     unsigned int out_len = 0;
//     unsigned int out_total = 0;

//     body_len = strlen(body);

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);
//     strcpy(objectId, "my_object");

//     kii.kii_core.response_code = 0;
//     ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     memset(uploadId, 0x00, KII_UPLOADID_SIZE + 1);
//     ret = kii_object_init_upload_body(&kii, &bucket, objectId, uploadId);

//     ASSERT_EQ(0, ret);
//     ASSERT_NE(0, strlen(uploadId));
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     chunk.chunk = body;
//     chunk.body_content_type = CONTENT_TYPE;
//     chunk.position = 0;
//     chunk.length = chunk.total_length = body_len;

//     kii.kii_core.response_code = 0;
//     ret = kii_object_upload_body(&kii, &bucket, objectId, uploadId, &chunk);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_commit_upload(&kii, &bucket, objectId, uploadId, 1);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_download_body(&kii, &bucket, objectId, 0, body_len,
//             &out_len, &out_total);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(body_len, out_len);
//     ASSERT_EQ(body_len, out_total);
//     ASSERT_STREQ(body, kii.kii_core.response_body);
//     ASSERT_EQ(206, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, pushBucket)
// {
//     int ret = -1;
//     char buffer[4096];
//     kii_t kii;
//     kii_bucket_t bucket;

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);

//     kii.kii_core.response_code = 0;
//     ret = kii_push_subscribe_bucket(&kii, &bucket);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_push_unsubscribe_bucket(&kii, &bucket);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, pushTopic)
// {
//     int ret = -1;
//     char buffer[4096];
//     kii_t kii;
//     kii_topic_t topic;

//     init(&kii, buffer, 4096);
//     topic.scope = KII_SCOPE_THING;
//     topic.scope_id = THING_ID;
//     topic.topic_name = TOPIC;

//     kii.kii_core.response_code = 0;
//     ret = kii_push_create_topic(&kii, &topic);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_push_subscribe_topic(&kii, &topic);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_push_unsubscribe_topic(&kii, &topic);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_push_delete_topic(&kii, &topic);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, genericApis)
// {
//     char buffer[4096];
//     kii_t kii;
//     const char* EX_AUTH_VENDOR_ID = "1426830900";
//     const char* EX_AUTH_VENDOR_PASS = "1234";
//     kii_json_t kii_json;
//     kii_json_field_t fields[3];
//     char author_id[128];
//     char access_token[128];

//     memset(&kii_json, 0x00, sizeof(kii_json));
//     memset(fields, 0, sizeof(fields));
//     memset(author_id, 0x00, sizeof(author_id));
//     memset(access_token, 0x00, sizeof(access_token));

//     fields[0].name = "id";
//     fields[0].type = KII_JSON_FIELD_TYPE_STRING;
//     fields[0].field_copy.string = author_id;
//     fields[0].field_copy_buff_size = sizeof(author_id) / sizeof(author_id[0]);
//     fields[1].name = "access_token";
//     fields[1].type = KII_JSON_FIELD_TYPE_STRING;
//     fields[1].field_copy.string = access_token;
//     fields[1].field_copy_buff_size = sizeof(access_token) /
//             sizeof(access_token[0]);
//     fields[2].name = NULL;

//     init(&kii, buffer, 4096);
//     kii.kii_core.response_code = 0;
//     ASSERT_EQ(0, kii_api_call_start(&kii, "POST", "api/oauth2/token",
//                     "application/json", KII_FALSE));
//     ASSERT_EQ(0, kii_api_call_append_body(&kii,
//                     "{\"username\":\"VENDOR_THING_ID:",
//                     strlen("{\"username\":\"VENDOR_THING_ID:")));
//     ASSERT_EQ(0, kii_api_call_append_body(&kii, EX_AUTH_VENDOR_ID,
//                     strlen(EX_AUTH_VENDOR_ID)));
//     ASSERT_EQ(0, kii_api_call_append_body(&kii, "\",\"password\":\"",
//                     strlen("\",\"password\":\"")));
//     ASSERT_EQ(0, kii_api_call_append_body(&kii, EX_AUTH_VENDOR_PASS,
//                     strlen(EX_AUTH_VENDOR_PASS)));
//     ASSERT_EQ(0, kii_api_call_append_body(&kii, "\"}", strlen("\"}")));

//     ASSERT_EQ(0, kii_api_call_run(&kii));

//     ASSERT_EQ(200, kii.kii_core.response_code);

//     ASSERT_EQ(KII_JSON_PARSE_SUCCESS,
//             kii_json_read_object(&kii_json, kii.kii_core.response_body,
//                     kii.kii_core.http_context.buffer_size -
//                         (kii.kii_core.http_context.buffer -
//                                 kii.kii_core.response_body), fields));
//     ASSERT_STREQ("th.53ae324be5a0-6a8a-4e11-7cec-026e0383", author_id);
//     ASSERT_STRNE("", access_token);
// }

// TEST(kiiTest, objectBodyOnce_binary)
// {
//     int ret = -1;
//     char body[] = "\0\0\01\0\01";
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;
//     unsigned int body_len = sizeof(body);
//     unsigned int out_len = 0;

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);
//     strcpy(objectId, "my_object");

//     kii_object_delete(&kii, &bucket, objectId);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_upload_body_at_once(&kii, &bucket, objectId,
//             "application/octet-stream", body, body_len);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_download_body_at_once(&kii, &bucket, objectId, &out_len);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(body_len, out_len);
//     ASSERT_EQ(0, memcmp(body, kii.kii_core.response_body, body_len));
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

// TEST(kiiTest, objectBodyMulti_binary)
// {
//     int ret = -1;
//     char body[] = "\0\01\0\0\0\01\0\01\0\01\0";
//     char buffer[4096];
//     char objectId[KII_OBJECTID_SIZE + 1];
//     char uploadId[KII_UPLOADID_SIZE + 1];
//     kii_t kii;
//     kii_bucket_t bucket;
//     kii_chunk_data_t chunk;
//     unsigned int body_len = 16;
//     unsigned int out_len = 0;
//     unsigned int out_total = 0;
//     char content_type[] = "application/octet-stream";

//     init(&kii, buffer, 4096);
//     initBucket(&bucket);
//     strcpy(objectId, "my_object");

//     kii_object_delete(&kii, &bucket, objectId);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(201, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     memset(uploadId, 0x00, KII_UPLOADID_SIZE + 1);
//     ret = kii_object_init_upload_body(&kii, &bucket, objectId, uploadId);

//     ASSERT_EQ(0, ret);
//     ASSERT_NE(0, strlen(uploadId));
//     ASSERT_EQ(200, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     chunk.chunk = body;
//     chunk.body_content_type = content_type;
//     chunk.position = 0;
//     chunk.total_length = body_len;
//     chunk.length = 5;

//     kii.kii_core.response_code = 0;
//     ret = kii_object_upload_body(&kii, &bucket, objectId, uploadId, &chunk);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     chunk.chunk = body + 5;
//     chunk.position = 5;
//     chunk.length = 11;
//     kii.kii_core.response_code = 0;
//     ret = kii_object_upload_body(&kii, &bucket, objectId, uploadId, &chunk);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_commit_upload(&kii, &bucket, objectId, uploadId, 1);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_download_body(&kii, &bucket, objectId, 0, body_len,
//             &out_len, &out_total);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(body_len, out_len);
//     ASSERT_EQ(body_len, out_total);
//     ASSERT_EQ(0, memcmp(body, kii.kii_core.response_body, body_len));
//     ASSERT_EQ(206, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

//     kii.kii_core.response_code = 0;
//     ret = kii_object_delete(&kii, &bucket, objectId);

//     ASSERT_EQ(0, ret);
//     ASSERT_EQ(204, kii.kii_core.response_code);
//     ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
//     ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
// }

/*
void received_callback(kii_t* kii, char* buffer, size_t buffer_size) {
    printf("%d\n%s\n", buffer_size, buffer);
    EXPECT_TRUE(NULL != buffer);
    EXPECT_STRNE("", buffer);
    EXPECT_NE(0, buffer_size);

    sleep(1);
    pthread_exit(NULL);
}

TEST(kiiTest, startRoutine)
{
    int ret = -1;
    char buffer[4096];
    char mqtt_buffer[2048];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    kii.mqtt_buffer = mqtt_buffer;
    kii.mqtt_buffer_size = 2048;
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    printf("Y\n");fflush(stdout);
    ret = kii_push_subscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);

    printf("Z\n");fflush(stdout);
    ret = kii_push_start_routine(&kii, received_callback);

    ASSERT_EQ(0, ret);

    printf("A\n");fflush(stdout);
    sleep(10);

    printf("B\n");fflush(stdout);
    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    printf("C\n");fflush(stdout);
    sleep(10);

    printf("D\n");fflush(stdout);
    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);

    printf("E\n");fflush(stdout);
    ret = kii_push_unsubscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);

    printf("F\n");fflush(stdout);
    sleep(10);
}
*/
