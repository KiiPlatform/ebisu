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
