#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <jkii.h>
#include <jkii_utils.h>
#include <math.h>
#include <limits.h>

static jkii_resource_t* cb_alloc(size_t required_size)
{
    jkii_resource_t* res =
        (jkii_resource_t*)malloc(sizeof(jkii_resource_t));
    if (res == NULL) {
        return NULL;
    }
    jkii_token_t *tokens =
        (jkii_token_t*)malloc(sizeof(jkii_token_t) * required_size);
    if (tokens == NULL) {
        free(res);
        return NULL;
    }
    res->tokens = tokens;
    res->tokens_num = required_size;
    return res;
}

static void cb_free(jkii_resource_t* resource) {
    free(resource->tokens);
    free(resource);
}


static jkii_resource_t* allocate_cb_fail(
        size_t required_size)
{
    return NULL;
}

TEST_CASE("KiiJson, GetObjectStringByName") {
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];

    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(json_string, strlen(json_string), fields, &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, GetObjectPositiveIntByName") {
    const char json_string[] = "{\"key1\" : 100}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];

    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectNegativeIntByName") {
    const char json_string[] = "{\"key1\" : -100}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];

    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectPositiveLongByName") {
    const char json_string[] = "{\"key1\" : 1099511627776}";
    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectNegativeLongByName") {
    const char json_string[] = "{\"key1\" : -1099511627776}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectPositiveDotDoubleByName") {
    const char json_string[] = "{\"key1\" : 0.1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(fields[0].field_copy.double_value == 0.1);
}

TEST_CASE("KiiJson, GetObjectNegativeDotDoubleByName") {
    const char json_string[] = "{\"key1\" : -0.1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(fields[0].field_copy.double_value == -0.1);
}

TEST_CASE("KiiJson, GetObjectPositiveEDoubleByName") {
    const char json_string[] = "{\"key1\" : 1e-1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectNegativeEDoubleByName") {
    const char json_string[] = "{\"key1\" : -1e-1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectTrueByName") {
    const char json_string[] = "{\"key1\" : true}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_TRUE == fields[0].field_copy.boolean_value);
}

TEST_CASE("KiiJson, GetObjectFalseByName") {
    const char json_string[] = "{\"key1\" : false}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FALSE == fields[0].field_copy.boolean_value);
}


TEST_CASE("KiiJson, GetObjectStringByPath") {
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, GetObjectPositiveIntByPath") {
    const char json_string[] = "{\"key1\" : 100}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectNegativeIntByPath") {
    const char json_string[] = "{\"key1\" : -100}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectPositiveLongByPath") {
    const char json_string[] = "{\"key1\" : 1099511627776}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectNegativeLongByPath") {
    const char json_string[] = "{\"key1\" : -1099511627776}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectPositiveDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : 0.1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectNegativeDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : -0.1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectPositiveEDoubleByPath") {
    const char json_string[] = "{\"key1\" : 1e-1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectNegativeEDoubleByPath") {
    const char json_string[] = "{\"key1\" : -1e-1}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectNullByPath") {
    const char json_string[] = "{\"key1\" : null}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_NULL;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
}

TEST_CASE("KiiJson, GetObjectTrueByPath") {
    const char json_string[] = "{\"key1\" : true}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_TRUE == fields[0].field_copy.boolean_value);
}

TEST_CASE("KiiJson, GetObjectFalseByPath") {
    const char json_string[] = "{\"key1\" : false}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FALSE == fields[0].field_copy.boolean_value);
}

TEST_CASE("KiiJson, GetObjectSecondLayerStringByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : \"value1\"}}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, GetObjectSecondLayerPositiveIntByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : 100}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectSecondLayerNegativeIntByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : -100}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectSecondLayerPositiveLongByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : 1099511627776}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectSecondLayerNegativeLongByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : -1099511627776}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectSecondLayerPositiveDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : 0.1}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectSecondLayerNegativeDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : -0.1}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectSecondLayerPositiveEDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : 1e-1}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectSecondLayerNegativeEDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : -1e-1}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectThirdLayerStringByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : \"value1\"}}}";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, GetObjectThirdLayerPositiveIntByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 100}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectThirdLayerNegativeIntByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -100}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetObjectThirdLayerPositiveLongByPath") {
    const char json_string[] =
        "{\"key1\" : {\"key2\" : {\"key3\" : 1099511627776}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectThirdLayerNegativeLongByPath") {
    const char json_string[] =
        "{\"key1\" : {\"key2\" : {\"key3\" : -1099511627776}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(-1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetObjectThirdLayerPositiveDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 0.1}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetObjectThirdLayerNegativeDotDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -0.1}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value + 0.1));
}

TEST_CASE("KiiJson, GetObjectThirdLayerPositiveEDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : 1e-1}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - (1e-1)));
}

TEST_CASE("KiiJson, GetObjectThirdLayerNegativeEDoubleByPath") {
    const char json_string[] = "{\"key1\" : {\"key2\" : {\"key3\" : -1e-1}}}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/key1/key2/key3";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - (-1e-1)));
}

TEST_CASE("KiiJson, GetArrayString") {
    const char json_string[] = "[\"value1\"]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, GetArrayInt") {
    const char json_string[] = "[100]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetArrayLong") {
    const char json_string[] = "[1099511627776]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_LONG;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1099511627776 == fields[0].field_copy.long_value);
}

TEST_CASE("KiiJson, GetArrayDouble") {
    const char json_string[] = "[1e-1]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_DOUBLE;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0.0001 >= fabs(fields[0].field_copy.double_value - 0.1));
}

TEST_CASE("KiiJson, GetArrayIntIndex1") {
    const char json_string[] = "[0, 100]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[1]";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(100 == fields[0].field_copy.int_value);
}

TEST_CASE("KiiJson, GetArrayNull") {
    const char json_string[] = "[null]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_NULL;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
}

TEST_CASE("KiiJson, GetArrayTrue") {
    const char json_string[] = "[true]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_TRUE == fields[0].field_copy.boolean_value);
}

TEST_CASE("KiiJson, GetArrayFalse") {
    const char json_string[] = "[false]";
    char buf[256];

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/[0]";
    fields[0].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FALSE == fields[0].field_copy.boolean_value);
}

TEST_CASE("KiiJson, GetComplexObject") {
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

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[15];
    memset(fields, 0x00, sizeof(fields));

    fields[0].path = "/parent1/child1-1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = child1_1buf;
    fields[0].field_copy_buff_size =
        sizeof(child1_1buf) / sizeof(child1_1buf[0]);
    fields[1].path = "/parent1/child1-2";
    fields[1].type = JKII_FIELD_TYPE_INTEGER;
    fields[2].path = "/parent1/child1-3";
    fields[2].type = JKII_FIELD_TYPE_LONG;
    fields[3].path = "/parent1/child1-4";
    fields[3].type = JKII_FIELD_TYPE_DOUBLE;
    fields[4].path = "/parent1/child1-5";
    fields[4].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[5].path = "/parent1/child1-6";
    fields[5].type = JKII_FIELD_TYPE_NULL;
    fields[6].path = "/parent2/[0]";
    fields[6].type = JKII_FIELD_TYPE_INTEGER;
    fields[7].path = "/parent2/[1]";
    fields[7].type = JKII_FIELD_TYPE_LONG;
    fields[8].path = "/parent2/[2]";
    fields[8].type = JKII_FIELD_TYPE_DOUBLE;
    fields[9].path = "/parent2/[3]";
    fields[9].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[10].path = "/parent2/[4]";
    fields[10].type = JKII_FIELD_TYPE_NULL;
    fields[11].path = "/parent2/[5]/child2-1";
    fields[11].type = JKII_FIELD_TYPE_STRING;
    fields[11].field_copy.string = child2_1buf;
    fields[11].field_copy_buff_size =
        sizeof(child2_1buf) / sizeof(child2_1buf[0]);
    fields[12].path = "/parent2/[5]/child2-2/[0]";
    fields[12].type = JKII_FIELD_TYPE_BOOLEAN;
    fields[13].path = "/parent3/first/second/third";
    fields[13].type = JKII_FIELD_TYPE_STRING;
    fields[13].field_copy.string = value_buf;
    fields[13].field_copy_buff_size =
        sizeof(value_buf) / sizeof(value_buf[0]);
    fields[14].path = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[1].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[2].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[3].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[4].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[5].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[6].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[7].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[8].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[9].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[10].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[11].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[12].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[13].result);

    REQUIRE(0 == strcmp("child value", fields[0].field_copy.string));
    REQUIRE(100 == fields[1].field_copy.int_value);
    REQUIRE(1099511627776 == fields[2].field_copy.long_value);
    REQUIRE(0.0001 >= fabs(fields[3].field_copy.double_value - 100.0e100));
    REQUIRE(JKII_TRUE == fields[4].field_copy.boolean_value);
    // fields[5] does not have value. it is null.
    REQUIRE(-100 == fields[6].field_copy.int_value);
    REQUIRE(-1099511627776 == fields[7].field_copy.long_value);
    REQUIRE(0.0001 >= fabs(fields[8].field_copy.double_value + 100.0e-100));
    REQUIRE(JKII_FALSE == fields[9].field_copy.boolean_value);
    REQUIRE(0 == strcmp("child value", fields[11].field_copy.string));
    REQUIRE(JKII_TRUE == fields[12].field_copy.boolean_value);
    // fields[12] does not have value. it is null.
    REQUIRE(0 == strcmp("value", fields[13].field_copy.string));
}

TEST_CASE("KiiJson, PushRetrieveEndpoint") {
    const char json_string[] =
        "{"
            "\"installationID\" : \"XXXXXXXXXXXXXXXXXXXXXXXXX\","
            "\"username\" : \"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\","
            "\"password\" : \"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\","
            "\"mqttTopic\" : \"XXXXXXXXXXXXXXXXXXXXXXX\","
            "\"host\" : \"XXXXXXXXXXXXXXXXXXXX.kii.com\","
            "\"portTCP\" : 1883,"
            "\"portSSL\" : 8883,"
            "\"X-MQTT-TTL\" : 2147483647"
        "}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[8];
    char username[64];
    char password[128];
    char topic[64];
    char host[64];

    memset(fields, 0, sizeof(fields));
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));
    memset(topic, 0, sizeof(topic));
    memset(host, 0, sizeof(host));
    fields[0].name = "username";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = username;
    fields[0].field_copy_buff_size = sizeof(username) / sizeof(username[0]);
    fields[1].name = "password";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = password;
    fields[1].field_copy_buff_size = sizeof(password) / sizeof(password[0]);
    fields[2].name = "host";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[2].field_copy.string = host;
    fields[2].field_copy_buff_size = sizeof(host) / sizeof(host[0]);
    fields[3].name = "mqttTopic";
    fields[3].type = JKII_FIELD_TYPE_STRING;
    fields[3].field_copy.string = topic;
    fields[3].field_copy_buff_size = sizeof(topic) / sizeof(topic[0]);
    fields[4].name = "portTCP";
    fields[4].type = JKII_FIELD_TYPE_INTEGER;
    fields[5].name = "portSSL";
    fields[5].type = JKII_FIELD_TYPE_INTEGER;
    fields[6].name = "X-MQTT-TTL";
    fields[6].type = JKII_FIELD_TYPE_LONG;
    fields[7].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[1].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[2].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[3].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[4].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[5].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[6].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[7].result);
}

TEST_CASE("KiiJson, CommandParseTest") {
    const char json_string[] =
        "{"
            "\"schema\":\"XXXXXXXXXXXXXX\","
            "\"schemaVersion\":1,"
            "\"commandID\":\"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\","
            "\"actions\":[{\"turnPower\":{\"power\":true}},"
                         "{\"setBrightness\":{\"brightness\":3000}},"
                         "{\"setColor\":{\"color\":[0,128,255]}},"
                         "{\"setColorTemperature\":{\"colorTemperature\":-100}}"
                         "]"
        "}";

    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };

    jkii_field_t fields[5];
    char schema[64];
    char commandID[64];

    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/schema";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = schema;
    fields[0].field_copy_buff_size = sizeof(schema) / sizeof(schema[0]);
    fields[1].path = "/schemaVersion";
    fields[1].type = JKII_FIELD_TYPE_INTEGER;
    fields[2].path = "/commandID";
    fields[2].type = JKII_FIELD_TYPE_STRING;
    fields[2].field_copy.string = commandID;
    fields[2].field_copy_buff_size =
        sizeof(commandID) / sizeof(commandID[0]);
    fields[3].path = "/actions";
    fields[3].type = JKII_FIELD_TYPE_ARRAY;
    fields[4].path = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[1].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[2].result);
    REQUIRE(JKII_FIELD_ERR_OK == fields[3].result);
}

TEST_CASE("KiiJson, NoTokensTest")
{
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            NULL);
    REQUIRE(JKII_ERR_TOKENS_SHORTAGE == res);
}

TEST_CASE("KiiJson, TokensShortageTest")
{
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_token_t tokens[1];
    jkii_resource_t resource = { tokens, 1 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_TOKENS_SHORTAGE == res);
}

TEST_CASE("KiiJson, ExactTokensTest")
{
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_token_t tokens[3];
    jkii_resource_t resource = { tokens, 3 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));

    REQUIRE((jkii_token_t*)NULL != resource.tokens);
    REQUIRE(3 == resource.tokens_num);
}

TEST_CASE("KiiJson, AllocatorTest")
{
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse_with_allocator(
            json_string,
            strlen(json_string),
            fields,
            cb_alloc,
            cb_free);

    REQUIRE(JKII_ERR_OK == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(0 == strcmp("value1", fields[0].field_copy.string));
}

TEST_CASE("KiiJson, FailedAllocationTest")
{
    const char json_string[] = "{\"key1\" : \"value1\"}";
    char buf[256];

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;


    jkii_parse_err_t res = jkii_parse_with_allocator(
            json_string,
            strlen(json_string),
            fields,
            allocate_cb_fail,
            cb_free);

    REQUIRE(JKII_ERR_ALLOCATION == res);
}
