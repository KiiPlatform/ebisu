#include <catch.hpp>
#include <jkii.h>
#include <jkii_utils.h>
#include <math.h>
#include <stdint.h>
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

// The ranges are those of int32_t and int64_t, whatever the widths of int and
// long. These went by INT_MAX and LONG_MAX, so where long is 32 bits LONG_MAX
// was an int and the long tests failed.
TEST_CASE("KiiJson, PrimitiveIntMax")
{
    const char str[] = "2147483647";
    jkii_primitive_t res;
    REQUIRE(JKII_PRIMITIVE_ERR_OK ==
            jkii_parse_primitive(str, strlen(str), &res));

    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);
    REQUIRE(INT32_MAX == res.value.int_value);
}

TEST_CASE("KiiJson, PrimitiveIntMin")
{
    const char str[] = "-2147483648";
    jkii_primitive_t res;
    REQUIRE(JKII_PRIMITIVE_ERR_OK ==
            jkii_parse_primitive(str, strlen(str), &res));

    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);
    REQUIRE(INT32_MIN == res.value.int_value);
}

TEST_CASE("KiiJson, PrimitiveLongMin")
{
    const char str[] = "-9223372036854775808";
    jkii_primitive_t res;
    REQUIRE(JKII_PRIMITIVE_ERR_OK ==
            jkii_parse_primitive(str, strlen(str), &res));

    REQUIRE(JKII_FIELD_TYPE_LONG == res.type);
    REQUIRE(INT64_MIN == res.value.long_value);
}

TEST_CASE("KiiJson, PrimitiveLongMax")
{
    const char str[] = "9223372036854775807";
    jkii_primitive_t res;
    REQUIRE(JKII_PRIMITIVE_ERR_OK ==
            jkii_parse_primitive(str, strlen(str), &res));

    REQUIRE(JKII_FIELD_TYPE_LONG == res.type);
    REQUIRE(INT64_MAX == res.value.long_value);
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

// A prefix of a literal must not be accepted as that literal. These used to
// pass, because the comparison was memcmp(buf, "null", buf_len): with buf_len
// shorter than the literal it only ever compared a prefix.
TEST_CASE("KiiJson, PrimitivePrefixOfLiteralIsNotThatLiteral")
{
    jkii_primitive_t res;

    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("n", 1, &res));
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("nul", 3, &res));
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("t", 1, &res));
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("tru", 3, &res));
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("f", 1, &res));
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID == jkii_parse_primitive("fals", 4, &res));
}

// The mirror image: a buffer longer than the literal must not be compared
// against bytes past the end of the literal. Reading ten bytes out of a
// five-byte "false" is what AddressSanitizer reports as a global-buffer-overflow;
// valgrind cannot see it, since the overrun is in .rodata.
TEST_CASE("KiiJson, PrimitiveLongerThanLiteralDoesNotOverread")
{
    jkii_primitive_t res;

    const char long_digits[] = "2147483647";
    REQUIRE(JKII_PRIMITIVE_ERR_OK ==
            jkii_parse_primitive(long_digits, strlen(long_digits), &res));
    REQUIRE(JKII_FIELD_TYPE_INTEGER == res.type);

    const char truthy[] = "truelike";
    REQUIRE(JKII_PRIMITIVE_ERR_INVALID ==
            jkii_parse_primitive(truthy, strlen(truthy), &res));
}

// tio parses the values of command actions with this. A number needs digits.
// "-" and "" asserted on BSD and macOS, where strtol sets EINVAL when it
// converts nothing, and elsewhere "" came back as the integer 0. "1e" came
// back as the double 1 everywhere.
TEST_CASE("KiiJson, PrimitiveNumberWithoutDigitsIsInvalid")
{
    const char* const values[] = { "", "-", "+", ".", "-.", "1e", "1e+", "-1E-" };

    for (const char* value : values) {
        INFO("value \"" << value << "\"");
        jkii_primitive_t res;
        CHECK(JKII_PRIMITIVE_ERR_INVALID ==
                jkii_parse_primitive(value, strlen(value), &res));
    }
}

// A JSON number has no leading zeros. jkii's integer check lets them through,
// and strtol was called with base 0, which reads a leading 0 as octal: 010
// came back as 8 and 08 was invalid.
TEST_CASE("KiiJson, PrimitiveIntegerWithLeadingZeroIsInvalid")
{
    const char* const zeros[] = { "0", "-0" };
    for (const char* value : zeros) {
        INFO("value " << value);
        jkii_primitive_t res;
        jkii_primitive_err_t err =
            jkii_parse_primitive(value, strlen(value), &res);
        CHECK(JKII_PRIMITIVE_ERR_OK == err);
        if (err == JKII_PRIMITIVE_ERR_OK) {
            CHECK(JKII_FIELD_TYPE_INTEGER == res.type);
            CHECK(0 == res.value.int_value);
        }
    }

    // The last is too long for an int64_t: invalid all the same, not an
    // overflow.
    const char* const values[] = {
        "00", "007", "08", "-08", "09", "010", "-010",
        "00000000000000000000000000000005"
    };
    for (const char* value : values) {
        INFO("value " << value);
        jkii_primitive_t res;
        CHECK(JKII_PRIMITIVE_ERR_INVALID ==
                jkii_parse_primitive(value, strlen(value), &res));
    }
}

// A byte above 0x7f is not a digit, wherever it is in a number. Where char
// is signed it is negative, which isdigit is not given.
TEST_CASE("KiiJson, PrimitiveNumberWithNonAsciiByteIsInvalid")
{
    const char* const values[] = {
        "\xb9", "1\xb9", "-\xb9", "1.\xb9", "1.5\xb9", "1e\xb9", "1e5\xb9",
        "\xff", "1\xff", "1.\xff", "1e\xff"
    };
    for (const char* value : values) {
        INFO("value " << value);
        jkii_primitive_t res;
        CHECK(JKII_PRIMITIVE_ERR_INVALID ==
                jkii_parse_primitive(value, strlen(value), &res));
    }
}

// Nor hexadecimal. These never reached strtol, whose base 0 would have read
// them, because the type check refuses the x; now strtol would not either.
TEST_CASE("KiiJson, PrimitiveHexadecimalIsInvalid")
{
    const char* const values[] = { "0x10", "-0x10", "0X10" };

    for (const char* value : values) {
        INFO("value " << value);
        jkii_primitive_t res;
        CHECK(JKII_PRIMITIVE_ERR_INVALID ==
                jkii_parse_primitive(value, strlen(value), &res));
    }
}
