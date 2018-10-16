#include <catch.hpp>
#include <jkii.h>
#include <jkii_utils.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <float.h>

TEST_CASE("KiiJson, PrimitiveInt")
{
    const char int_str[] = "1";
    jkii_primitive_t res;
    jkii_parse_primitive(int_str, 1, &res);

    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);
    REQUIRE(1 == res.value.int_value);
}

TEST_CASE("KiiJson, PrimitiveIntMax")
{
    char str_buff[64];
    snprintf(str_buff, 64, "%d", INT_MAX);
    jkii_primitive_t res;
    jkii_parse_primitive(str_buff, strlen(str_buff), &res);

    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);
    REQUIRE(INT_MAX == res.value.int_value);
}

TEST_CASE("KiiJson, PrimitiveIntMin")
{
    char str_buff[64];
    snprintf(str_buff, 64, "%d", INT_MIN);
    jkii_primitive_t res;
    jkii_parse_primitive(str_buff, strlen(str_buff), &res);

    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);
    REQUIRE(INT_MIN == res.value.int_value);
}

TEST_CASE("KiiJson, PrimitiveLongMin")
{
    char str_buff[64];
    snprintf(str_buff, 64, "%ld", LONG_MIN);
    jkii_primitive_t res;
    jkii_parse_primitive(str_buff, strlen(str_buff), &res);

    REQUIRE(JKII_FIELD_TYPE_LONG == res.type);
    REQUIRE(LONG_MIN == res.value.long_value);
}

TEST_CASE("KiiJson, PrimitiveLongMax")
{
    char str_buff[64];
    snprintf(str_buff, 64, "%ld", LONG_MAX);
    jkii_primitive_t res;
    jkii_parse_primitive(str_buff, strlen(str_buff), &res);

    REQUIRE(JKII_FIELD_TYPE_LONG == res.type);
    REQUIRE(LONG_MAX == res.value.long_value);
}

TEST_CASE("KiiJson, PrimitiveDouble")
{
    const char double_str[] = "0.1";
    jkii_primitive_t res;
    jkii_parse_primitive(double_str, strlen(double_str), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(0.1 == res.value.double_value);
}

TEST_CASE("KiiJson, PrimitiveDoubleScientific")
{
    const char double_str[] = "1e-1";
    jkii_primitive_t res;
    jkii_parse_primitive(double_str, strlen(double_str), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(0.1 == res.value.double_value);

    memset(&res, 0, sizeof(jkii_primitive_t));
    const char double_str2[] = "1e+1";
    jkii_parse_primitive(double_str2, strlen(double_str2), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(10.0 == res.value.double_value);

    memset(&res, 0, sizeof(jkii_primitive_t));
    const char double_str3[] = "1e2";
    jkii_parse_primitive(double_str3, strlen(double_str3), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(100.0 == res.value.double_value);
}

TEST_CASE("KiiJson, PrimitiveDoubleScientificCap")
{
    const char double_str[] = "1E-1";
    jkii_primitive_t res;
    jkii_parse_primitive(double_str, strlen(double_str), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(0.1 == res.value.double_value);

    memset(&res, 0, sizeof(jkii_primitive_t));
    const char double_str2[] = "1E+1";
    jkii_parse_primitive(double_str2, strlen(double_str2), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(10.0 == res.value.double_value);

    memset(&res, 0, sizeof(jkii_primitive_t));
    const char double_str3[] = "1E2";
    jkii_parse_primitive(double_str3, strlen(double_str3), &res);

    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);
    REQUIRE(100.0 == res.value.double_value);
}

TEST_CASE("KiiJson, PrimitiveDoubleMin")
{
    char str_buff[64];
    int written = snprintf(str_buff, 64, "%le", -DBL_MAX);
    REQUIRE(written < 64);
    jkii_primitive_t res;
    jkii_primitive_err_t pres =
        jkii_parse_primitive(str_buff, strlen(str_buff), &res);
    REQUIRE(JKII_PRIMITIVE_ERR_OK == pres);
    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);

    double expect = strtod(str_buff, NULL);
    REQUIRE(expect == res.value.double_value);
}

TEST_CASE("KiiJson, PrimitiveDoubleMax")
{
    char str_buff[64];
    int written = snprintf(str_buff, 64, "%le", DBL_MAX);
    REQUIRE(written < 64);
    jkii_primitive_t res;
    jkii_primitive_err_t pres =
        jkii_parse_primitive(str_buff, strlen(str_buff), &res);
    REQUIRE(JKII_PRIMITIVE_ERR_OK == pres);
    REQUIRE(JKII_FIELD_TYPE_DOUBLE == res.type);

    double expect = strtod(str_buff, NULL);
    REQUIRE(expect == res.value.double_value);
}

TEST_CASE("KiiJson, PrimitiveNull")
{
    const char null_str[] = "null";
    jkii_primitive_t res;
    jkii_parse_primitive(null_str, strlen(null_str), &res);

    REQUIRE(JKII_FIELD_TYPE_NULL == res.type);
}

TEST_CASE("KiiJson, PrimitiveTrue")
{
    const char bool_str[] = "true";
    jkii_primitive_t res;
    jkii_parse_primitive(bool_str, strlen(bool_str), &res);

    REQUIRE(JKII_FIELD_TYPE_BOOLEAN == res.type);
    REQUIRE(JKII_TRUE == res.value.boolean_value);
}

TEST_CASE("KiiJson, PrimitiveFalse")
{
    const char bool_str[] = "false";
    jkii_primitive_t res;
    jkii_parse_primitive(bool_str, strlen(bool_str), &res);

    REQUIRE(JKII_FIELD_TYPE_BOOLEAN == res.type);
    REQUIRE(JKII_FALSE == res.value.boolean_value);
}
