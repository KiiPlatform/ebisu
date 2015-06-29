#include <gtest/gtest.h>
#include <kii.h>
#include <kii_json.h>
#include <math.h>

TEST(KiiJson, GetObjectStringByName) {
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(0, strcmp("value1", fields[0].field_copy.string));
}

TEST(KiiJson, GetObjectPositiveIntByName) {
    const char json_string[] = "{\"key1\" : 100}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectNegativeIntByName) {
    const char json_string[] = "{\"key1\" : -100}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectPositiveLongByName) {
    const char json_string[] = "{\"key1\" : 1099511627776}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectNegativeLongByName) {
    const char json_string[] = "{\"key1\" : -1099511627776}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectPositiveDotDoubleByName) {
    const char json_string[] = "{\"key1\" : 0.1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectNegativeDotDoubleByName) {
    const char json_string[] = "{\"key1\" : -0.1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectPositiveEDoubleByName) {
    const char json_string[] = "{\"key1\" : 1e-1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectNegativeEDoubleByName) {
    const char json_string[] = "{\"key1\" : -1e-1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectTrueByName) {
    const char json_string[] = "{\"key1\" : true}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_TRUE, fields[0].field_copy.boolean_value);
}

TEST(KiiJson, GetObjectFalseByName) {
    const char json_string[] = "{\"key1\" : false}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_FALSE, fields[0].field_copy.boolean_value);
}


TEST(KiiJson, GetObjectStringByPath) {
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(0, strcmp("value1", fields[0].field_copy.string));
}

TEST(KiiJson, GetObjectPositiveIntByPath) {
    const char json_string[] = "{\"key1\" : 100}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectNegativeIntByPath) {
    const char json_string[] = "{\"key1\" : -100}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectPositiveLongByPath) {
    const char json_string[] = "{\"key1\" : 1099511627776}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectNegativeLongByPath) {
    const char json_string[] = "{\"key1\" : -1099511627776}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectPositiveDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : 0.1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectNegativeDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : -0.1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectPositiveEDoubleByPath) {
    const char json_string[] = "{\"key1\" : 1e-1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectNegativeEDoubleByPath) {
    const char json_string[] = "{\"key1\" : -1e-1}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectNullByPath) {
    const char json_string[] = "{\"key1\" : null}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_NULL;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
}

TEST(KiiJson, GetObjectTrueByPath) {
    const char json_string[] = "{\"key1\" : true}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_TRUE, fields[0].field_copy.boolean_value);
}

TEST(KiiJson, GetObjectFalseByPath) {
    const char json_string[] = "{\"key1\" : false}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_FALSE, fields[0].field_copy.boolean_value);
}

TEST(KiiJson, GetObjectSecondLayerStringByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : \"value1\"}}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(0, strcmp("value1", fields[0].field_copy.string));
}

TEST(KiiJson, GetObjectSecondLayerPositiveIntByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : 100}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectSecondLayerNegativeIntByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : -100}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectSecondLayerPositiveLongByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : 1099511627776}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectSecondLayerNegativeLongByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : -1099511627776}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectSecondLayerPositiveDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : 0.1}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectSecondLayerNegativeDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : -0.1}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectSecondLayerPositiveEDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : 1e-1}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectSecondLayerNegativeEDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : -1e-1}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectThirdLayerStringByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : \"value1\"}}}";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(0, strcmp("value1", fields[0].field_copy.string));
}

TEST(KiiJson, GetObjectThirdLayerPositiveIntByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 100}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectThirdLayerNegativeIntByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -100}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetObjectThirdLayerPositiveLongByPath) {
    const char json_string[] =
        "{\"key1\" : {\"key2\" : {\"key3\" : 1099511627776}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectThirdLayerNegativeLongByPath) {
    const char json_string[] =
        "{\"key1\" : {\"key2\" : {\"key3\" : -1099511627776}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(-1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetObjectThirdLayerPositiveDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 0.1}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectThirdLayerNegativeDotDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -0.1}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetObjectThirdLayerPositiveEDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 1e-1}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetObjectThirdLayerNegativeEDoubleByPath) {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -1e-1}}}";
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value + 0.1));
}

TEST(KiiJson, GetArrayString) {
    const char json_string[] = "[\"value1\"]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(0, strcmp("value1", fields[0].field_copy.string));
}

TEST(KiiJson, GetArrayInt) {
    const char json_string[] = "[100]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetArrayLong) {
    const char json_string[] = "[1099511627776]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(1099511627776, fields[0].field_copy.long_value);
}

TEST(KiiJson, GetArrayDouble) {
    const char json_string[] = "[1e-1]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_GE(0.0001, fabs(fields[0].field_copy.double_value - 0.1));
}

TEST(KiiJson, GetArrayIntIndex1) {
    const char json_string[] = "[0, 100]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[1]";
    fields[0].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(100, fields[0].field_copy.int_value);
}

TEST(KiiJson, GetArrayNull) {
    const char json_string[] = "[null]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_NULL;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
}

TEST(KiiJson, GetArrayTrue) {
    const char json_string[] = "[true]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_TRUE, fields[0].field_copy.boolean_value);
}

TEST(KiiJson, GetArrayFalse) {
    const char json_string[] = "[false]";
    char buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[2];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_FALSE, fields[0].field_copy.boolean_value);
}

TEST(KiiJson, GetComplexObject) {
    const char json_string[] =
        "{"
            "\"parent1\" : {"
                "\"child1-1\" : \"child value\","
            "\"child1-2\" : 100,"
            "\"child1-3\" : 1099511627776,"
            "\"child1-4\" : 100.0e100,"
            "\"child1-5\" : true,"
            "\"child1-6\" : null"
            "},"
            "\"parent2\" : ["
            "-100,"
            "-1099511627776,"
            "-100.0e-100,"
            "false,"
            "null,"
            "{"
                "\"child2-1\" : \"child value\","
                "\"child2-2\" : [true],"
            "}"
            "]"
            "\"parent3\" : {"
            "\"first\" : { \"second\" : { \"third\" : \"value\" } }"
            "}"
        "}";
    char child1_1buf[256];
    char child2_1buf[256];
    char value_buf[256];
    kii_json_t kii_json;
    kii_json_field_t fields[15];

    memset(&kii_json, 0x00, sizeof(kii_json));
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/parent1/child1-1";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = child1_1buf;
    fields[0].field_copy_buff_size =
        sizeof(child1_1buf) / sizeof(child1_1buf[0]);
    fields[1].path = "/parent1/child1-2";
    fields[1].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[2].path = "/parent1/child1-3";
    fields[2].type = KII_JSON_FIELD_TYPE_LONG;
    fields[3].path = "/parent1/child1-4";
    fields[3].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[4].path = "/parent1/child1-5";
    fields[4].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[5].path = "/parent1/child1-6";
    fields[5].type = KII_JSON_FIELD_TYPE_NULL;
    fields[6].path = "/parent2/[0]";
    fields[6].type = KII_JSON_FIELD_TYPE_INTEGER;
    fields[7].path = "/parent2/[1]";
    fields[7].type = KII_JSON_FIELD_TYPE_LONG;
    fields[8].path = "/parent2/[2]";
    fields[8].type = KII_JSON_FIELD_TYPE_DOUBLE;
    fields[9].path = "/parent2/[3]";
    fields[9].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[10].path = "/parent2/[4]";
    fields[10].type = KII_JSON_FIELD_TYPE_NULL;
    fields[11].path = "/parent2/[5]/child2-1";
    fields[11].type = KII_JSON_FIELD_TYPE_STRING;
    fields[11].field_copy.string = child2_1buf;
    fields[11].field_copy_buff_size =
        sizeof(child2_1buf) / sizeof(child2_1buf[0]);
    fields[12].path = "/parent2/[5]/child2-2/[0]";
    fields[12].type = KII_JSON_FIELD_TYPE_BOOLEAN;
    fields[13].path = "/parent3/first/second/third";
    fields[13].type = KII_JSON_FIELD_TYPE_STRING;
    fields[13].field_copy.string = value_buf;
    fields[13].field_copy_buff_size =
        sizeof(value_buf) / sizeof(value_buf[0]);
    fields[14].path = NULL;

    EXPECT_EQ(KII_JSON_PARSE_SUCCESS,
            kii_json_read_object(
                &kii_json,
                json_string,
                sizeof(json_string) / sizeof(json_string[0]),
                fields));
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[0].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[1].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[2].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[3].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[4].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[5].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[6].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[7].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[7].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[9].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[10].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[11].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[12].result);
    EXPECT_EQ(KII_JSON_FIELD_PARSE_SUCCESS, fields[13].result);

    EXPECT_EQ(0, strcmp("child value", fields[0].field_copy.string));
    EXPECT_EQ(100, fields[1].field_copy.int_value);
    EXPECT_EQ(1099511627776, fields[2].field_copy.long_value);
    EXPECT_GE(0.0001, fabs(fields[3].field_copy.double_value - 100.0e100));
    EXPECT_EQ(KII_JSON_TRUE, fields[4].field_copy.boolean_value);
    // fields[5] does not have value. it is null.
    EXPECT_EQ(-100, fields[6].field_copy.int_value);
    EXPECT_EQ(-1099511627776, fields[7].field_copy.long_value);
    EXPECT_GE(0.0001, fabs(fields[8].field_copy.double_value + 100.0e-100));
    EXPECT_EQ(KII_JSON_FALSE, fields[9].field_copy.boolean_value);
    EXPECT_EQ(0, strcmp("child value", fields[11].field_copy.string));
    EXPECT_EQ(KII_JSON_TRUE, fields[12].field_copy.boolean_value);
    // fields[12] does not have value. it is null.
    EXPECT_EQ(0, strcmp("value", fields[13].field_copy.string));
}
