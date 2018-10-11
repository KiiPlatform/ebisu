#include <gtest/gtest.h>
#include <kii_json.h>
#include <kii_json_utils.h>
#include <math.h>
#include <limits.h>
#include <string.h>

TEST(KiiJson, PrimitiveInt)
{
    const char int_str[] = "1";
    kii_json_primitive_t res;
    kii_json_parse_primitive(int_str, 1, &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_INTEGER, res.type);
    EXPECT_EQ(1, res.value.int_value);
}

TEST(KiiJson, PrimitiveIntMax)
{
    char str_buff[64];
    snprintf(str_buff, 64, "%d", INT_MAX);
    kii_json_primitive_t res;
    kii_json_parse_primitive(str_buff, strlen(str_buff), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_INTEGER, res.type);
    EXPECT_EQ(INT_MAX, res.value.int_value);
}

TEST(KiiJson, PrimitiveIntMin)
{
    char str_buff[64];
    snprintf(str_buff, 64, "%d", INT_MIN);
    kii_json_primitive_t res;
    kii_json_parse_primitive(str_buff, strlen(str_buff), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_INTEGER, res.type);
    EXPECT_EQ(INT_MIN, res.value.int_value);
}

TEST(KiiJson, PrimitiveLongMin)
{
    char str_buff[64];
    snprintf(str_buff, 64, "%ld", LONG_MIN);
    kii_json_primitive_t res;
    kii_json_parse_primitive(str_buff, strlen(str_buff), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_LONG, res.type);
    EXPECT_EQ(LONG_MIN, res.value.long_value);
}

TEST(KiiJson, PrimitiveLongMax)
{
    char str_buff[64];
    snprintf(str_buff, 64, "%ld", LONG_MAX);
    kii_json_primitive_t res;
    kii_json_parse_primitive(str_buff, strlen(str_buff), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_LONG, res.type);
    EXPECT_EQ(LONG_MAX, res.value.long_value);
}

TEST(KiiJson, PrimitiveDouble)
{
    const char double_str[] = "0.1";
    kii_json_primitive_t res;
    kii_json_parse_primitive(double_str, strlen(double_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(0.1, res.value.double_value);
}

TEST(KiiJson, PrimitiveDoubleScientific)
{
    const char double_str[] = "1e-1";
    kii_json_primitive_t res;
    kii_json_parse_primitive(double_str, strlen(double_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(0.1, res.value.double_value);

    memset(&res, 0, sizeof(kii_json_primitive_t));
    const char double_str2[] = "1e+1";
    kii_json_parse_primitive(double_str2, strlen(double_str2), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(10.0, res.value.double_value);

    memset(&res, 0, sizeof(kii_json_primitive_t));
    const char double_str3[] = "1e2";
    kii_json_parse_primitive(double_str3, strlen(double_str3), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(100.0, res.value.double_value);
}

TEST(KiiJson, PrimitiveDoubleScientificCap)
{
    const char double_str[] = "1E-1";
    kii_json_primitive_t res;
    kii_json_parse_primitive(double_str, strlen(double_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(0.1, res.value.double_value);

    memset(&res, 0, sizeof(kii_json_primitive_t));
    const char double_str2[] = "1E+1";
    kii_json_parse_primitive(double_str2, strlen(double_str2), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(10.0, res.value.double_value);

    memset(&res, 0, sizeof(kii_json_primitive_t));
    const char double_str3[] = "1E2";
    kii_json_parse_primitive(double_str3, strlen(double_str3), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);
    EXPECT_EQ(100.0, res.value.double_value);
}

TEST(KiiJson, PrimitiveDoubleMin)
{
    char str_buff[64];
    int written = snprintf(str_buff, 64, "%le", -DBL_MAX);
    EXPECT_LT(written, 64);
    kii_json_primitive_t res;
    kii_json_parse_primitive_result_t pres =
        kii_json_parse_primitive(str_buff, strlen(str_buff), &res);
    EXPECT_EQ(KII_JSON_PARSE_PRIMITIVE_RESULT_SUCCESS, pres);
    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);

    double expect = strtod(str_buff, NULL);
    EXPECT_DOUBLE_EQ(expect, res.value.double_value);
}

TEST(KiiJson, PrimitiveDoubleMax)
{
    char str_buff[64];
    int written = snprintf(str_buff, 64, "%le", DBL_MAX);
    EXPECT_LT(written, 64);
    kii_json_primitive_t res;
    kii_json_parse_primitive_result_t pres =
        kii_json_parse_primitive(str_buff, strlen(str_buff), &res);
    EXPECT_EQ(KII_JSON_PARSE_PRIMITIVE_RESULT_SUCCESS, pres);
    EXPECT_EQ(KII_JSON_FIELD_TYPE_DOUBLE, res.type);

    double expect = strtod(str_buff, NULL);
    EXPECT_DOUBLE_EQ(expect, res.value.double_value);
}

TEST(KiiJson, PrimitiveNull)
{
    const char null_str[] = "null";
    kii_json_primitive_t res;
    kii_json_parse_primitive(null_str, strlen(null_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_NULL, res.type);
}

TEST(KiiJson, PrimitiveTrue)
{
    const char bool_str[] = "true";
    kii_json_primitive_t res;
    kii_json_parse_primitive(bool_str, strlen(bool_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_BOOLEAN, res.type);
    EXPECT_EQ(KII_JSON_TRUE, res.value.boolean_value);
}

TEST(KiiJson, PrimitiveFalse)
{
    const char bool_str[] = "false";
    kii_json_primitive_t res;
    kii_json_parse_primitive(bool_str, strlen(bool_str), &res);

    EXPECT_EQ(KII_JSON_FIELD_TYPE_BOOLEAN, res.type);
    EXPECT_EQ(KII_JSON_FALSE, res.value.boolean_value);
}