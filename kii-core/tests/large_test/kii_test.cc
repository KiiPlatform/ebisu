#include <stdio.h>

#include <kii_json.h>

// Suppress warnings in gtest.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"
#pragma GCC diagnostic ignored "-Wlong-long"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <kii_core_init.h>

#define KII_OBJECTID_SIZE 36

#define KII_UPLOADID_SIZE 64

static char APP_HOST[] = "api-development-jp.internal.kii.com";
static char APP_ID[] = "84fff36e";
static char APP_KEY[] = "e45fcc2d31d6aca675af639bc5f04a26";
static char THING_ID[] = "th.53ae324be5a0-26f8-4e11-a13c-03da6fb2";
static char ACCESS_TOKEN[] = "ablTGrnsE20rSRBFKPnJkWyTaeqQ50msqUizvR_61hU";
static char BUCKET[] = "myBucket";
static char TOPIC[] = "myTopic";
static char DUMMY_HEADER[] = "DummyHeader:DummyValue";

static void init(
        kii_core_t* kii,
        char* buffer,
        int buffer_size)
{
    kii_core_impl_init(kii, APP_HOST, APP_ID, APP_KEY, buffer, buffer_size);

    strcpy(kii->author.author_id, THING_ID);
    strcpy(kii->author.access_token, ACCESS_TOKEN);
}

static void initBucket(kii_bucket_t* bucket)
{
    bucket->scope = KII_SCOPE_THING;
    bucket->scope_id = THING_ID;
    bucket->bucket_name = BUCKET;
}

TEST(kiiTest, authenticate)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    kii_core_t kii;

    init(&kii, buffer, 4096);

    strcpy(kii.author.author_id, "");
    strcpy(kii.author.access_token, "");
    kii.response_code = 0;

    core_err = kii_core_thing_authentication(&kii, "1426830900", "1234");
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STRNE("", kii.response_body);

    ASSERT_TRUE(strstr(kii.response_body, "\"id\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"access_token\"") != NULL);
}

TEST(kiiTest, register)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    char thingData[1024];
    kii_core_t kii;

    sprintf(thingData,
            "{\"_vendorThingID\":\"%d\",\"_password\":\"1234\",\"_thingType\":\"my_type\"}",
            getpid());

    init(&kii, buffer, 4096);

    strcpy(kii.author.author_id, "");
    strcpy(kii.author.access_token, "");
    kii.response_code = 0;

    core_err = kii_core_register_thing(&kii, thingData);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(201, kii.response_code);
    ASSERT_STRNE("", kii.response_body);

    ASSERT_TRUE(strstr(kii.response_body, "\"_accessToken\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"_thingID\"") != NULL);
}

TEST(kiiTest, register_with_id)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    char vendorId[1024];
    kii_core_t kii;

    sprintf(vendorId, "%d", getpid() + 1);

    init(&kii, buffer, 4096);

    strcpy(kii.author.author_id, "");
    strcpy(kii.author.access_token, "");
    kii.response_code = 0;

    core_err = kii_core_register_thing_with_id(&kii, vendorId, "1234",
            "my_type");
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(201, kii.response_code);
    ASSERT_STRNE("", kii.response_body);

    ASSERT_TRUE(strstr(kii.response_body, "\"_accessToken\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"_thingID\"") != NULL);
}

TEST(kiiTest, object)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    char *pObjectId = NULL;
    kii_core_t kii;
    kii_bucket_t bucket;

    memset(objectId, 0x00, KII_OBJECTID_SIZE + 1);

    init(&kii, buffer, 4096);
    initBucket(&bucket);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_create_new_object(&kii, &bucket, "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(201, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    pObjectId = strstr(kii.response_body, "\"objectID\"");
    ASSERT_TRUE(pObjectId != NULL);
    strncpy(objectId, pObjectId + 14, 36);
    ASSERT_NE("", objectId);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_patch_object(&kii, &bucket, objectId, "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_replace_object(&kii, &bucket, objectId, "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_get_object(&kii, &bucket, objectId);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_TRUE(NULL != kii.response_body);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_delete_object(&kii, &bucket, objectId);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);
}

TEST(kiiTest, objectWithID)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    char objectId[KII_OBJECTID_SIZE + 1];
    kii_core_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    initBucket(&bucket);
    strcpy(objectId, "my_object");

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_create_new_object_with_id(&kii, &bucket, objectId,
            "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(201, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_patch_object(&kii, &bucket, objectId, "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_replace_object(&kii, &bucket, objectId, "{}", NULL);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_get_object(&kii, &bucket, objectId);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STRNE("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_delete_object(&kii, &bucket, objectId);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);
}

TEST(kiiTest, pushBucket)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    kii_core_t kii;
    kii_bucket_t bucket;

    init(&kii, buffer, 4096);
    initBucket(&bucket);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_subscribe_bucket(&kii, &bucket);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_unsubscribe_bucket(&kii, &bucket);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);
}

TEST(kiiTest, pushTopic)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    kii_core_t kii;
    kii_topic_t topic;

    init(&kii, buffer, 4096);
    topic.scope = KII_SCOPE_THING;
    topic.scope_id = THING_ID;
    topic.topic_name = TOPIC;

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_create_topic(&kii, &topic);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_subscribe_topic(&kii, &topic);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_unsubscribe_topic(&kii, &topic);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_delete_topic(&kii, &topic);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(204, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_STREQ("", kii.response_body);
}

TEST(kiiTest, genericApis)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    kii_core_t kii;
    const char* EX_AUTH_VENDOR_ID = "1426830900";
    const char* EX_AUTH_VENDOR_PASS = "1234";

    init(&kii, buffer, 4096);
    kii.response_code = 0;
    kii.response_body = NULL;

    ASSERT_EQ(KIIE_OK, kii_core_api_call_start(&kii, "POST", "api/oauth2/token",
                "application/json", KII_FALSE));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_header(&kii,
                "DummyHeader", "DummyValue"));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_body(&kii,
                "{\"username\":\"VENDOR_THING_ID:",
                strlen("{\"username\":\"VENDOR_THING_ID:")));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_body(&kii, EX_AUTH_VENDOR_ID,
                strlen(EX_AUTH_VENDOR_ID)));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_body(&kii, "\",\"password\":\"",
                strlen("\",\"password\":\"")));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_body(&kii, EX_AUTH_VENDOR_PASS,
                strlen(EX_AUTH_VENDOR_PASS)));
    ASSERT_EQ(KIIE_OK, kii_core_api_call_append_body(&kii, "\"}",
                strlen("\"}")));

    ASSERT_EQ(KIIE_OK, kii_core_api_call_end(&kii));

    ASSERT_TRUE(strstr(buffer, "DummyHeader:DummyValue") != NULL);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"id\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"access_token\"") != NULL);
}

TEST(kiiTest, genericApis2)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    kii_core_t kii;

    init(&kii, buffer, 4096);
    kii.response_code = 0;
    kii.response_body = NULL;

    ASSERT_EQ(KIIE_OK, kii_core_api_call(&kii, "POST", "api/oauth2/token",
                "{\"username\":\"VENDOR_THING_ID:1426830900\",\"password\":\"1234\"}",
                59, "application/json", DUMMY_HEADER, NULL));

    ASSERT_TRUE(strstr(buffer, DUMMY_HEADER) != NULL);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"id\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"access_token\"") != NULL);
}

TEST(kiiTest, mqtt)
{
    kii_error_code_t core_err;
    kii_state_t state;
    char buffer[4096];
    char id[64];
    kii_core_t kii;
    char *pId = NULL;

    memset(id, 0, sizeof(id));
    init(&kii, buffer, 4096);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_install_thing_push(&kii, KII_TRUE);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(201, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    pId = strstr(kii.response_body, "\"installationID\"");
    ASSERT_TRUE(pId != NULL);
    strncpy(id, pId + 20, 25);
    ASSERT_NE("", id);

    kii.response_code = 0;
    kii.response_body = NULL;
    core_err = kii_core_get_mqtt_endpoint(&kii, id);
    ASSERT_EQ(KIIE_OK, core_err);

    do {
        core_err = kii_core_run(&kii);
        state = kii_core_get_state(&kii);
    } while (state != KII_STATE_IDLE);

    ASSERT_EQ(KIIE_OK, core_err);
    ASSERT_EQ(200, kii.response_code);
    ASSERT_STREQ(THING_ID, kii.author.author_id);
    ASSERT_STREQ(ACCESS_TOKEN, kii.author.access_token);
    ASSERT_TRUE(kii.response_body != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"username\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"password\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"host\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"mqttTopic\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"portTCP\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"portSSL\"") != NULL);
    ASSERT_TRUE(strstr(kii.response_body, "\"X-MQTT-TTL\"") != NULL);
}

TEST(kiiTest, api_calls_upload_binary)
{
    char buffer[4096];
    kii_core_t kii;
    kii_bucket_t bucket;
    char object_id[256];
    const char upload_data[] = "\01\0\0\0\0\0\01\0";

    init(&kii, buffer, 4096);
    initBucket(&bucket);

    // create object
    kii.response_code = 0;
    kii.response_body = NULL;

    ASSERT_EQ(KIIE_OK,
            kii_core_create_new_object(&kii, &bucket, "{}", NULL));

    do {
        kii_error_code_t error = kii_core_run(&kii);
        if (error == KIIE_FAIL) {
            ASSERT_TRUE(false);
        }
    } while (kii_core_get_state(&kii) != KII_STATE_IDLE);
    ASSERT_EQ(201, kii.response_code);

    {
        kii_json_t kii_json;
        kii_json_field_t fields[2];
        char error_string_buff[256];

        memset(&kii_json, 0, sizeof(kii_json));
        memset(error_string_buff, 0,
                sizeof(error_string_buff) / sizeof(error_string_buff[0]));

        kii_json.error_string_buff = error_string_buff;
        kii_json.error_string_length =
            sizeof(error_string_buff) / sizeof(error_string_buff[0]);

        memset(fields, 0, sizeof(fields));
        fields[0].name = "objectID";
        fields[0].type = KII_JSON_FIELD_TYPE_STRING;
        fields[0].field_copy.string = object_id;
        fields[0].field_copy_buff_size =
            sizeof(object_id) / sizeof(object_id[0]);
        fields[1].name = NULL;

        ASSERT_EQ(KII_JSON_PARSE_SUCCESS,
                kii_json_read_object(&kii_json,
                        kii.response_body,
                        strlen(kii.response_body),
                        fields));
    }

    // init upload
    kii.response_code = 0;
    kii.response_body = NULL;

    {
        char path[256];

        memset(path, 0, sizeof(path));
        sprintf(path,
                "api/apps/%s/things/%s/buckets/%s/objects/%s/body/uploads",
                APP_ID, bucket.scope_id, bucket.bucket_name, object_id);
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_start(
                &kii,
                "POST",
                path,
                "application/vnd.kii.startobjectbodyuploadrequest+json",
                KII_TRUE));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_append_header(
                &kii,
                "accept",
                "application/vnd.kii.startobjectbodyuploadresponse+json"));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_append_body(&kii, "{}", 2));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_end(&kii));

        do {
            kii_error_code_t error = kii_core_run(&kii);
            if (error == KIIE_FAIL) {
                ASSERT_TRUE(false);
            }
        } while (kii_core_get_state(&kii) != KII_STATE_IDLE);
        ASSERT_EQ(200, kii.response_code);
    }

    // upload once.
    kii.response_code = 0;
    kii.response_body = NULL;

    {
        char path[256];

        sprintf(path,
                "api/apps/%s/things/%s/buckets/%s/objects/%s/body",
                APP_ID, bucket.scope_id, bucket.bucket_name, object_id);

        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_start(
                &kii,
                "PUT",
                path,
                "application/octet-stream",
                KII_TRUE));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_append_body(
                &kii,
                upload_data,
                sizeof(upload_data)));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_end(&kii));
        do {
            kii_error_code_t error = kii_core_run(&kii);
            if (error == KIIE_FAIL) {
                ASSERT_TRUE(false);
            }
        } while (kii_core_get_state(&kii) != KII_STATE_IDLE);
        ASSERT_EQ(200, kii.response_code);
    }

    // get body.
    kii.response_code = 0;
    kii.response_body = NULL;

    {
        char path[256];
        sprintf(
            path,
            "api/apps/%s/things/%s/buckets/%s/objects/%s/body",
            APP_ID, bucket.scope_id, bucket.bucket_name, object_id);
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_start(
                &kii,
                "GET",
                path,
                NULL,
                KII_TRUE));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_append_header(
                &kii,
                "accept",
                "*/*"));
        ASSERT_EQ(
            KIIE_OK,
            kii_core_api_call_end(&kii));
        do {
            kii_error_code_t error = kii_core_run(&kii);
            if (error == KIIE_FAIL) {
                ASSERT_TRUE(false);
            }
        } while (kii_core_get_state(&kii) != KII_STATE_IDLE);
        ASSERT_EQ(200, kii.response_code);
        ASSERT_EQ(
            0,
            memcmp(
                upload_data,
                kii.response_body,
                sizeof(upload_data)));
    }
    // delete object
    kii.response_code = 0;
    kii.response_body = NULL;

    ASSERT_EQ(KIIE_OK,
            kii_core_delete_object(&kii, &bucket, object_id));
    do {
        kii_error_code_t error = kii_core_run(&kii);
        if (error == KIIE_FAIL) {
            ASSERT_TRUE(false);
        }
    } while (kii_core_get_state(&kii) != KII_STATE_IDLE);
    ASSERT_EQ(204, kii.response_code);
}
