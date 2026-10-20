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

// Parses {"key1" : value}, asking for key1 as the given type.
static jkii_parse_err_t parse_number_by_name(
        const char* value,
        jkii_field_type_t type,
        jkii_field_t* out_field)
{
    char json_string[64];
    snprintf(json_string, sizeof(json_string), "{\"key1\" : %s}", value);

    jkii_token_t tokens[8];
    jkii_resource_t resource = { tokens, 8 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = type;
    fields[1].name = NULL;

    jkii_parse_err_t res = jkii_parse(
            json_string,
            strlen(json_string),
            fields,
            &resource);
    *out_field = fields[0];
    return res;
}

// A sign, a point or an exponent marker without the digits that go with it is
// not a number, but jsmn passes any primitive through, so one can arrive in a
// command or a response. "-" passes jkii's type check as an integer and "1e"
// as a double, so they reach strtol and strtod, which convert nothing of "-"
// and only the "1" of "1e". Whether strtol reports that in errno is left to
// the libc: glibc does not, BSD and macOS set EINVAL, which jkii asserted
// against. "1e" was read as 1 and reported as parsed, on every libc.
TEST_CASE("KiiJson, GetObjectNumberWithoutDigitsByName") {
    const char* const values[] = { "-", "+", ".", "-.", "1e", "1e+", "-1E-" };
    const jkii_field_type_t types[] = {
        JKII_FIELD_TYPE_INTEGER,
        JKII_FIELD_TYPE_LONG,
        JKII_FIELD_TYPE_DOUBLE
    };

    for (const char* value : values) {
        for (jkii_field_type_t type : types) {
            INFO("value " << value << ", type " << type);
            jkii_field_t field;
            jkii_parse_err_t res = parse_number_by_name(value, type, &field);
            // Either the input is refused, or this field is reported failed.
            if (res != JKII_ERR_INVALID_INPUT) {
                CHECK(JKII_ERR_PARTIAL == res);
                CHECK(JKII_FIELD_ERR_OK != field.result);
            }
        }
    }
}

// Their neighbours that do have digits still parse.
TEST_CASE("KiiJson, GetObjectNumberWithDigitsByName") {
    jkii_field_t field;

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("-1", JKII_FIELD_TYPE_INTEGER, &field));
    REQUIRE(-1 == field.field_copy.int_value);

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("-0", JKII_FIELD_TYPE_LONG, &field));
    REQUIRE(0 == field.field_copy.long_value);

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("1e1", JKII_FIELD_TYPE_DOUBLE, &field));
    REQUIRE(10.0 == field.field_copy.double_value);

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("1e+1", JKII_FIELD_TYPE_DOUBLE, &field));
    REQUIRE(10.0 == field.field_copy.double_value);

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("-1E-1", JKII_FIELD_TYPE_DOUBLE, &field));
    REQUIRE(0.0001 >= fabs(field.field_copy.double_value + 0.1));

    REQUIRE(JKII_ERR_OK ==
            parse_number_by_name("-0.5", JKII_FIELD_TYPE_DOUBLE, &field));
    REQUIRE(-0.5 == field.field_copy.double_value);
}

// A JSON number has no leading zeros. jkii's integer check lets them through,
// and strtol was called with base 0, which reads a leading 0 as octal: 010
// came back as 8, and 08, not being octal, made the whole input invalid.
TEST_CASE("KiiJson, GetObjectIntegerWithLeadingZeroByName") {
    const char* const zeros[] = { "0", "-0" };
    const char* const values[] = {
        "00", "007", "08", "-08", "09", "010", "-010"
    };
    const jkii_field_type_t types[] = {
        JKII_FIELD_TYPE_INTEGER,
        JKII_FIELD_TYPE_LONG
    };

    for (const char* value : zeros) {
        INFO("value " << value);
        jkii_field_t field;

        CHECK(JKII_ERR_OK ==
                parse_number_by_name(value, JKII_FIELD_TYPE_INTEGER, &field));
        CHECK(0 == field.field_copy.int_value);

        CHECK(JKII_ERR_OK ==
                parse_number_by_name(value, JKII_FIELD_TYPE_LONG, &field));
        CHECK(0 == field.field_copy.long_value);
    }

    for (const char* value : values) {
        for (jkii_field_type_t type : types) {
            INFO("value " << value << ", type " << type);
            jkii_field_t field;
            jkii_parse_err_t res = parse_number_by_name(value, type, &field);
            // Either the input is refused, or this field is reported failed.
            if (res != JKII_ERR_INVALID_INPUT) {
                CHECK(JKII_ERR_PARTIAL == res);
                CHECK(JKII_FIELD_ERR_OK != field.result);
            }
        }
    }
}

// Nor hexadecimal. These never reached strtol, whose base 0 would have read
// them, because the type check refuses the x; now strtol would not either.
TEST_CASE("KiiJson, GetObjectHexadecimalByName") {
    const char* const values[] = { "0x10", "-0x10", "0X10" };
    const jkii_field_type_t types[] = {
        JKII_FIELD_TYPE_INTEGER,
        JKII_FIELD_TYPE_LONG
    };

    for (const char* value : values) {
        for (jkii_field_type_t type : types) {
            INFO("value " << value << ", type " << type);
            jkii_field_t field;
            jkii_parse_err_t res = parse_number_by_name(value, type, &field);
            // Either the input is refused, or this field is reported failed.
            if (res != JKII_ERR_INVALID_INPUT) {
                CHECK(JKII_ERR_PARTIAL == res);
                CHECK(JKII_FIELD_ERR_OK != field.result);
            }
        }
    }
}

TEST_CASE("KiiJson, GetObjectTrueByName") {
    const char json_string[] = "{\"key1\" : true}";

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

// An index in a path goes through strtol as well. Without digits it converts
// nothing, which asserted on BSD and macOS. Elsewhere "/[]", which hands
// strtol an empty string and so leaves nothing over, found element 0.
TEST_CASE("KiiJson, GetArrayIndexWithoutDigits") {
    const char json_string[] = "[10]";
    const char* const paths[] = { "/[]", "/[-]" };

    for (const char* path : paths) {
        INFO("path " << path);

        jkii_token_t tokens[8];
        jkii_resource_t resource = { tokens, 8 };

        jkii_field_t fields[2];
        memset(fields, 0x00, sizeof(fields));

        fields[0].path = path;
        fields[0].type = JKII_FIELD_TYPE_INTEGER;
        fields[1].name = NULL;

        jkii_parse_err_t res = jkii_parse(
                json_string,
                strlen(json_string),
                fields,
                &resource);
        CHECK(JKII_ERR_PARTIAL == res);
        CHECK(JKII_FIELD_ERR_NOT_FOUND == fields[0].result);
    }
}

// An index in a path follows the same rule, as the path grammar in jkii.h has
// it: /[010] found element 8, and now finds nothing, as /[08] did.
TEST_CASE("KiiJson, GetArrayIndexWithLeadingZero") {
    // Each element is its own index.
    const char json_string[] = "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]";
    const struct {
        const char* path;
        int index; // -1: not found
    } cases[] = {
        { "/[0]", 0 }, { "/[8]", 8 }, { "/[10]", 10 }, { "/[11]", 11 },
        { "/[00]", -1 }, { "/[08]", -1 }, { "/[09]", -1 },
        { "/[010]", -1 }, { "/[011]", -1 }, { "/[0x1]", -1 }
    };

    for (const auto& c : cases) {
        INFO("path " << c.path);

        jkii_token_t tokens[16];
        jkii_resource_t resource = { tokens, 16 };

        jkii_field_t fields[2];
        memset(fields, 0x00, sizeof(fields));

        fields[0].path = c.path;
        fields[0].type = JKII_FIELD_TYPE_INTEGER;
        fields[1].name = NULL;

        jkii_parse_err_t res = jkii_parse(
                json_string,
                strlen(json_string),
                fields,
                &resource);
        if (c.index < 0) {
            CHECK(JKII_ERR_PARTIAL == res);
            CHECK(JKII_FIELD_ERR_NOT_FOUND == fields[0].result);
        } else {
            CHECK(JKII_ERR_OK == res);
            CHECK(JKII_FIELD_ERR_OK == fields[0].result);
            CHECK(c.index == fields[0].field_copy.int_value);
        }
    }
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

// A string with no JSON token in it is invalid input, not a programming error:
// it can come from the network, e.g. a command whose alias maps to "" rather
// than to an array of actions. The entry points asserted a non-zero length,
// aborting any build with assertions enabled, and without them went on to read
// the token array as the previous parse had left it. Whitespace alone was
// never asserted against and took that second path in every build.
TEST_CASE("KiiJson, NoTokenInputTest")
{
    const char previous[] = "{\"key1\" : \"value1\"}";
    const char empty[] = "";
    const char whitespace[] = " \t\r\n";
    char buf[256];

    jkii_token_t tokens[8];
    jkii_resource_t resource = { tokens, 8 };

    jkii_field_t fields[2];
    memset(fields, 0x00, sizeof(fields));

    fields[0].name = "key1";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = buf;
    fields[0].field_copy_buff_size = sizeof(buf) / sizeof(buf[0]);
    fields[1].name = NULL;

    // Leave the tokens of a valid object behind, as kii and tio do by reusing
    // one resource for every parse.
    REQUIRE(JKII_ERR_OK ==
            jkii_parse(previous, strlen(previous), fields, &resource));

    SECTION("jkii_parse") {
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_parse(empty, 0, fields, &resource));
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_parse(whitespace, strlen(whitespace), fields, &resource));
    }

    SECTION("jkii_parse_with_allocator") {
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_parse_with_allocator(empty, 0, fields, cb_alloc, cb_free));
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_parse_with_allocator(whitespace, strlen(whitespace),
                    fields, cb_alloc, cb_free));
    }

    SECTION("jkii_validate_root_object") {
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_validate_root_object(empty, 0, &resource));
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_validate_root_object(whitespace, strlen(whitespace),
                    &resource));
    }

    SECTION("jkii_validate_root_object_with_allocator") {
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_validate_root_object_with_allocator(empty, 0,
                    cb_alloc, cb_free));
        REQUIRE(JKII_ERR_INVALID_INPUT ==
                jkii_validate_root_object_with_allocator(whitespace,
                    strlen(whitespace), cb_alloc, cb_free));
    }
}

// jsmn is built non-strict, so a key with no value is not a parse error:
// {"a"} is an object holding one string, with no token after it for the
// value. Each document below ends in such a key, and the token after the
// last one parsed must not be taken for its value.
//
// parse_after() parses into a resource that a previous message was parsed
// into, as kii and tio reuse theirs, and into the same buffer, left
// uncleared. That message had a value where this one stops, so the token
// after the last one parsed is still that value, its offsets still
// pointing at its text.
static jkii_parse_err_t parse_after(
        const char* previous,
        const char* json_string,
        jkii_field_t* fields)
{
    char buf[256];
    jkii_token_t tokens[256];
    jkii_resource_t resource = { tokens, 256 };
    jkii_field_t no_fields[1];
    memset(no_fields, 0x00, sizeof(no_fields));

    memcpy(buf, previous, strlen(previous));
    REQUIRE(JKII_ERR_OK ==
            jkii_parse(buf, strlen(previous), no_fields, &resource));
    // No terminator: the rest of the previous message stays after this one.
    memcpy(buf, json_string, strlen(json_string));
    return jkii_parse(buf, strlen(json_string), fields, &resource);
}

// The allocator variant sizes the token array to the parse, so there is no
// token after the last one.
static jkii_parse_err_t parse_allocated(
        const char* json_string,
        jkii_field_t* fields)
{
    return jkii_parse_with_allocator(json_string, strlen(json_string),
            fields, cb_alloc, cb_free);
}

TEST_CASE("KiiJson, OnlyKeyHasNoValue")
{
    const char json_string[] = "{\"a\"}";
    char by_name[64];
    char by_path[64];
    jkii_parse_err_t res = JKII_ERR_OK;

    jkii_field_t fields[3];
    memset(fields, 0x00, sizeof(fields));
    fields[0].name = "a";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = by_name;
    fields[0].field_copy_buff_size = sizeof(by_name) / sizeof(by_name[0]);
    fields[1].path = "/a";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = by_path;
    fields[1].field_copy_buff_size = sizeof(by_path) / sizeof(by_path[0]);
    fields[2].name = NULL;

    SECTION("reused resource") {
        res = parse_after("{\"a\":\"stale\"}", json_string, fields);
    }
    SECTION("allocator") {
        res = parse_allocated(json_string, fields);
    }
    REQUIRE(JKII_ERR_PARTIAL == res);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[0].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[1].result);
}

TEST_CASE("KiiJson, LastKeyHasNoValue")
{
    const char json_string[] = "{\"a\":1,\"b\"}";
    jkii_parse_err_t res = JKII_ERR_OK;

    jkii_field_t fields[6];
    memset(fields, 0x00, sizeof(fields));
    fields[0].name = "a";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    fields[1].name = "b";
    fields[1].type = JKII_FIELD_TYPE_ANY;
    fields[2].path = "/b";
    fields[2].type = JKII_FIELD_TYPE_ANY;
    // Not there at all, but looking for it walks past b.
    fields[3].name = "c";
    fields[3].type = JKII_FIELD_TYPE_ANY;
    fields[4].path = "/c";
    fields[4].type = JKII_FIELD_TYPE_ANY;
    fields[5].name = NULL;

    SECTION("reused resource") {
        res = parse_after("{\"a\":1,\"b\":\"stale\"}", json_string, fields);
    }
    SECTION("allocator") {
        res = parse_allocated(json_string, fields);
    }
    REQUIRE(JKII_ERR_PARTIAL == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(1 == fields[0].field_copy.int_value);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[1].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[2].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[3].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[4].result);
}

TEST_CASE("KiiJson, NestedKeyHasNoValue")
{
    const char json_string[] = "{\"a\":{\"b\"}}";
    jkii_parse_err_t res = JKII_ERR_OK;

    jkii_field_t fields[4];
    memset(fields, 0x00, sizeof(fields));
    fields[0].path = "/a";
    fields[0].type = JKII_FIELD_TYPE_OBJECT;
    fields[1].path = "/a/b";
    fields[1].type = JKII_FIELD_TYPE_ANY;
    fields[2].path = "/a/c";
    fields[2].type = JKII_FIELD_TYPE_ANY;
    fields[3].path = NULL;

    SECTION("reused resource") {
        res = parse_after("{\"a\":{\"b\":\"stale\"}}", json_string, fields);
    }
    SECTION("allocator") {
        res = parse_allocated(json_string, fields);
    }
    REQUIRE(JKII_ERR_PARTIAL == res);
    REQUIRE(JKII_FIELD_ERR_OK == fields[0].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[1].result);
    REQUIRE(JKII_FIELD_ERR_NOT_FOUND == fields[2].result);
}
