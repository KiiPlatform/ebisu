#include <stdio.h>
#include <gtest/gtest.h>

#include <kii.h>
#include "kii_core_impl.h"

#define THING_ID "th.53ae324be5a0-26f8-4e11-a13c-03da6fb2"

static void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        context_t* context)
{
    kii_init(kii, "api-development-jp.internal.kii.com",
            "84fff36e", "e45fcc2d31d6aca675af639bc5f04a26");

    kii->kii_core.http_context.buffer = buffer;
    kii->kii_core.http_context.buffer_size = buffer_size;
    kii->kii_core.http_context.app_context = context;
    kii->mqtt_socket_context.app_context = context;

    strcpy(kii->kii_core.author.author_id, THING_ID);
    strcpy(kii->kii_core.author.access_token,
            "ablTGrnsE20rSRBFKPnJkWyTaeqQ50msqUizvR_61hU");
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
    context_t context;

    init(&kii, buffer, 4096, &context);

    ret = kii_thing_authenticate(&kii, "1426830900", "1234");

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, register)
{
    int ret = -1;
    char buffer[4096];
    char vendorId[1024];
    kii_t kii;
    context_t context;

    sprintf(vendorId, "%d", getpid());

    init(&kii, buffer, 4096, &context);

    ret = kii_thing_register(&kii, vendorId, "my_type", "1234");

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, object)
{
    int ret = -1;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;
    context_t context;

    init(&kii, buffer, 4096, &context);
    initBucket(&bucket);

    memset(objectId, 0x00, KII_OBJECTID_SIZE + 1);
    ret = kii_object_create(&kii, &bucket, "{}", NULL, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_NE(0, strlen(objectId));

    ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_get(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_TRUE(NULL != kii.kii_core.response_body);

    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, objectWithID)
{
    int ret = -1;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;
    context_t context;

    init(&kii, buffer, 4096, &context);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_patch(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_replace(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_get(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
    ASSERT_TRUE(NULL != kii.kii_core.response_body);

    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, objectBodyOnce)
{
    int ret = -1;
    char* body = "Stop the world!";
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_t kii;
    kii_bucket_t bucket;
    context_t context;
    unsigned int body_len;
    unsigned int out_len = 0;

    body_len = strlen(body);

    init(&kii, buffer, 4096, &context);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    ret = kii_object_upload_body_at_once(&kii, &bucket, objectId, "text/plain",
            body, body_len);

    ASSERT_EQ(0, ret);

    ret = kii_object_download_body_at_once(&kii, &bucket, objectId, &out_len);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(body_len, out_len);
    ASSERT_STREQ(body, kii.kii_core.response_body);

    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
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
    context_t context;
    unsigned int body_len;
    unsigned int out_len = 0;
    unsigned int out_total = 0;

    body_len = strlen(body);

    init(&kii, buffer, 4096, &context);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    ret = kii_object_create_with_id(&kii, &bucket, objectId, "{}", NULL);

    ASSERT_EQ(0, ret);

    memset(uploadId, 0x00, KII_UPLOADID_SIZE + 1);
    ret = kii_object_init_upload_body(&kii, &bucket, objectId, uploadId);

    ASSERT_EQ(0, ret);
    ASSERT_NE(0, strlen(uploadId));

    chunk.chunk = body;
    chunk.body_content_type = "text/plain";
    chunk.position = 0;
    chunk.length = chunk.total_length = body_len;

    ret = kii_object_upload_body(&kii, &bucket, objectId, uploadId, &chunk);

    ASSERT_EQ(0, ret);

    ret = kii_object_commit_upload(&kii, &bucket, objectId, uploadId, 1);

    ASSERT_EQ(0, ret);

    ret = kii_object_download_body(&kii, &bucket, objectId, 0, body_len,
            &out_len, &out_total);

    ASSERT_EQ(0, ret);
    ASSERT_EQ(body_len, out_len);
    ASSERT_EQ(body_len, out_total);
    ASSERT_STREQ(body, kii.kii_core.response_body);

    ret = kii_object_delete(&kii, &bucket, objectId);

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, pushBucket)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    kii_bucket_t bucket;
    context_t context;

    init(&kii, buffer, 4096, &context);
    initBucket(&bucket);

    ret = kii_push_subscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);

    ret = kii_push_unsubscribe_bucket(&kii, &bucket);

    ASSERT_EQ(0, ret);
}

TEST(kiiTest, pushTopic)
{
    int ret = -1;
    char buffer[4096];
    kii_t kii;
    kii_topic_t topic;
    context_t context;

    init(&kii, buffer, 4096, &context);
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = THING_ID;
    topic.topic_name = "myTopic";

    ret = kii_push_create_topic(&kii, &topic);

    ASSERT_EQ(0, ret);

    ret = kii_push_subscribe_topic(&kii, &topic);

    ASSERT_EQ(0, ret);

    ret = kii_push_unsubscribe_topic(&kii, &topic);

    ASSERT_EQ(0, ret);

    ret = kii_push_delete_topic(&kii, &topic);

    ASSERT_EQ(0, ret);
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
    context_t context;

    init(&kii, buffer, 4096, &context);
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
