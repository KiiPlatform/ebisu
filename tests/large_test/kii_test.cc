#include <stdio.h>
#include <gtest/gtest.h>

#include <kii.h>
#include <kii_json.h>

#define THING_ID "th.53ae324be5a0-26f8-4e11-a13c-03da6fb2"
#define ACCESS_TOKEN "ablTGrnsE20rSRBFKPnJkWyTaeqQ50msqUizvR_61hU"

static void init(
        kii_t* kii,
        char* buffer,
        int buffer_size)
{
    kii_init(kii, "api-development-jp.internal.kii.com",
            "84fff36e", "e45fcc2d31d6aca675af639bc5f04a26");

    kii->kii_core.http_context.buffer = buffer;
    kii->kii_core.http_context.buffer_size = buffer_size;
    kii->mqtt_socket_context.app_context = NULL;

    strcpy(kii->kii_core.author.author_id, THING_ID);
    strcpy(kii->kii_core.author.access_token, ACCESS_TOKEN);
}

static void initBucket(kii_bucket_t* bucket)
{
    bucket->scope = KII_SCOPE_THING;
    bucket->scope_id = THING_ID;
    bucket->bucket_name = "myBucket";
}

TEST(kiiTest, authenticate)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;

    init(&kii, buffer, 4096);

    strcpy(kii.kii_core.author.author_id, "");
    strcpy(kii.kii_core.author.access_token, "");
    kii.kii_core.response_code = 0;
    ret = kii_thing_authenticate(&kii, "1426830900", "1234");

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STRNE("", kii.kii_core.author.author_id);
    ASSERT_STRNE("", kii.kii_core.author.access_token);
}

TEST(kiiTest, register)
{
    int ret = -1;
    char buffer[4096];
    char vendorId[1024];
    kii_t kii;

    sprintf(vendorId, "%d", getpid());

    init(&kii, buffer, 4096);

    strcpy(kii.kii_core.author.author_id, "");
    strcpy(kii.kii_core.author.access_token, "");
    kii.kii_core.response_code = 0;
    ret = kii_thing_register(&kii, vendorId, "my_type", "1234");

    ASSERT_EQ(0, ret);
    ASSERT_EQ(201, kii.kii_core.response_code);
    ASSERT_STRNE("", kii.kii_core.author.author_id);
    ASSERT_STRNE("", kii.kii_core.author.access_token);
}

TEST(kiiTest, object)
{
    int ret = -1;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    initBucket(&bucket);

    kii.kii_core.response_code = 0;
    memset(objectId, 0x00, KII_OBJECTID_SIZE + 1);
    ret = kii_object_create(&kii, &bucket, "{}", NULL, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_NE(0, strlen(objectId));
    ASSERT_EQ(201, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_get(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_TRUE(NULL != kii.kii_core.response_body);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, objectWithID)
{
    int ret = -1;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    kii.kii_core.response_code = 0;
    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(201, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_get(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
    ASSERT_TRUE(NULL != kii.kii_core.response_body);

    kii.kii_core.response_code = 0;
    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, objectBodyOnce)
{
    int ret = -1;
    char* body = "Stop the world!";
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;
    unsigned int body_len;
    unsigned int out_len = 0;

    body_len = strlen(body);

    init(&kii, buffer, 4096);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    kii.kii_core.response_code = 0;
    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(201, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_upload_body_at_once(&kii, &bucket, objectId, "text/plain",
            body, body_len);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_download_body_at_once(&kii, &bucket, objectId, &out_len);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(body_len, out_len);
    ASSERT_STREQ(body, kii.kii_core.response_body);
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, objectBodyMulti)
{
    int ret = -1;
    char* body = "Stop the world!";
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    char uploadId[KII_UPLOADID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;
    kii_chunk_data_t chunk;
    unsigned int body_len;
    unsigned int out_len = 0;
    unsigned int out_total = 0;

    body_len = strlen(body);

    init(&kii, buffer, 4096);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    kii.kii_core.response_code = 0;
    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(201, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    memset(uploadId, 0x00, KII_UPLOADID_SIZE + 1);
    ret = kii_object_init_upload_body(&kii, &bucket, objectId, uploadId);

    ASSERT_EQ(0, ret);
    ASSERT_NE(0, strlen(uploadId));
    ASSERT_EQ(200, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    chunk.chunk = body;
    chunk.body_content_type = "text/plain";
    chunk.position = 0;
    chunk.length = chunk.total_length = body_len;

    kii.kii_core.response_code = 0;
    ret = kii_object_upload_body(&kii, &bucket, objectId, uploadId, &chunk);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_commit_upload(&kii, &bucket, objectId, uploadId, 1);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_download_body(&kii, &bucket, objectId, 0, body_len,
            &out_len, &out_total);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(body_len, out_len);
    ASSERT_EQ(body_len, out_total);
    ASSERT_STREQ(body, kii.kii_core.response_body);
    ASSERT_EQ(206, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, pushBucket)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    initBucket(&bucket);

    kii.kii_core.response_code = 0;
    ret = kii_push_subscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_push_unsubscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, pushTopic)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    kii_topic_t topic;

    init(&kii, buffer, 4096);
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = THING_ID;
    topic.topic_name = "myTopic";

    kii.kii_core.response_code = 0;
    ret = kii_push_create_topic(&kii, &topic);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_push_subscribe_topic(&kii, &topic);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_push_unsubscribe_topic(&kii, &topic);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);

    kii.kii_core.response_code = 0;
    ret = kii_push_delete_topic(&kii, &topic);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(204, kii.kii_core.response_code);
    ASSERT_STREQ(THING_ID, kii.kii_core.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.kii_core.author.access_token);
}

TEST(kiiTest, genericApis)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    const char* EX_AUTH_VENDOR_ID = "1426830900";
    const char* EX_AUTH_VENDOR_PASS = "1234";
    kii_json_t kii_json;
    kii_json_field_t fields[3];
    char author_id[128];
    char access_token[128];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0, sizeof(fields));
    memset(author_id, 0x00, sizeof(author_id));
    memset(access_token, 0x00, sizeof(access_token));

    fields[0].name = "id";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = author_id;
    fields[0].field_copy_buff_size = sizeof(author_id) / sizeof(author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = access_token;
    fields[1].field_copy_buff_size = sizeof(access_token) /
            sizeof(access_token[0]);
    fields[2].name = NULL;

    init(&kii, buffer, 4096);
    kii.kii_core.response_code = 0;
    ASSERT_EQ(0, kii_api_call_start(&kii, "POST", "api/oauth2/token",
                    "application/json", KII_FALSE));
    ASSERT_EQ(0, kii_api_call_append_body(&kii,
                    "{\"username\":\"VENDOR_THING_ID:",
                    strlen("{\"username\":\"VENDOR_THING_ID:")));
    ASSERT_EQ(0, kii_api_call_append_body(&kii, EX_AUTH_VENDOR_ID,
                    strlen(EX_AUTH_VENDOR_ID)));
    ASSERT_EQ(0, kii_api_call_append_body(&kii, "\",\"password\":\"",
                    strlen("\",\"password\":\"")));
    ASSERT_EQ(0, kii_api_call_append_body(&kii, EX_AUTH_VENDOR_PASS,
                    strlen(EX_AUTH_VENDOR_PASS)));
    ASSERT_EQ(0, kii_api_call_append_body(&kii, "\"}", strlen("\"}")));

    ASSERT_EQ(0, kii_api_call_run(&kii));

    ASSERT_EQ(200, kii.kii_core.response_code);

    ASSERT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(&kii_json, kii.kii_core.response_body,
                    kii.kii_core.http_context.buffer_size -
                        (kii.kii_core.http_context.buffer -
                                kii.kii_core.response_body), fields));
    ASSERT_STREQ("th.53ae324be5a0-6a8a-4e11-7cec-026e0383", author_id);
    ASSERT_STRNE("", access_token);
}

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
    ret = kii_push_start_routine(&kii, 0, 0, received_callback);

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
