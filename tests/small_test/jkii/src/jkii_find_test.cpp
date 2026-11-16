// jkii_find_member(), and the reads of a member it, jkii_next_member() or
// jkii_root_value() gives: its string, unescaped, its number as an integer
// within a range or as a double, and its true or false; and, when asked, a
// number or a boolean in a string.
#include <catch.hpp>
#include <jkii.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

// A document as a response's buffer has it: not terminated.
std::vector<char> doc(const std::string& s)
{
    return std::vector<char>(s.begin(), s.end());
}

std::string value_of(const jkii_member_t& m)
{
    return std::string(m.value, m.value_length);
}

// A member that is the value as written, for the reads.
jkii_member_t number(const char* json)
{
    jkii_member_t m;
    REQUIRE( jkii_root_value(json, strlen(json), &m) == JKII_ERR_OK );
    return m;
}

} // namespace

TEST_CASE("jkii_find_member gives the value a path leads to") {
    const std::vector<char> d = doc(
            "{\"a\" : 1, \"s\" : \"x\\\"y\", \"o\" : {\"b\" : [10, {\"c\" : true}]},"
            " \"a/b\" : 2, \"[0]\" : 3, \"dup\" : 1, \"dup\" : 2, \"n\" : null} ");
    jkii_member_t m;

    REQUIRE( jkii_find_member(d.data(), d.size(), "/a", &m) == JKII_ERR_OK );
    CHECK( m.key == nullptr );
    CHECK( m.key_length == 0 );
    CHECK( value_of(m) == "1" );
    CHECK( m.type == JKII_FIELD_TYPE_INTEGER );

    // A string's characters between its quotes, as written.
    REQUIRE( jkii_find_member(d.data(), d.size(), "/s", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "x\\\"y" );
    CHECK( m.type == JKII_FIELD_TYPE_STRING );

    REQUIRE( jkii_find_member(d.data(), d.size(), "/o", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "{\"b\" : [10, {\"c\" : true}]}" );
    CHECK( m.type == JKII_FIELD_TYPE_OBJECT );
    REQUIRE( jkii_find_member(d.data(), d.size(), "/o/b/[0]", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "10" );
    REQUIRE( jkii_find_member(d.data(), d.size(), "/o/b/[1]/c", &m) == JKII_ERR_OK );
    CHECK( m.type == JKII_FIELD_TYPE_BOOLEAN );
    REQUIRE( jkii_find_member(d.data(), d.size(), "/n", &m) == JKII_ERR_OK );
    CHECK( m.type == JKII_FIELD_TYPE_NULL );

    // Names with the path's own characters, escaped.
    REQUIRE( jkii_find_member(d.data(), d.size(), "/a\\/b", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "2" );
    REQUIRE( jkii_find_member(d.data(), d.size(), "/\\[0\\]", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "3" );

    // The first of a name, as jkii_parse() takes it.
    REQUIRE( jkii_find_member(d.data(), d.size(), "/dup", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "1" );

    // Nothing there, or not a path: m as it was.
    m.value = nullptr;
    for (const char* path : { "/z", "/o/b/[2]", "/o/b/c", "/a/b/c", "a", "", "//a", "/o/b/[x]" }) {
        INFO("path " << path);
        CHECK( jkii_find_member(d.data(), d.size(), path, &m) == JKII_ERR_NOT_FOUND );
        CHECK( m.value == nullptr );
    }
}

TEST_CASE("jkii_find_member takes a root that is an object or an array, of JSON") {
    jkii_member_t m;
    const std::vector<char> array = doc("[1, [2, 3]]");
    REQUIRE( jkii_find_member(array.data(), array.size(), "/[1]/[0]", &m) == JKII_ERR_OK );
    CHECK( value_of(m) == "2" );
    for (const char* json : { "42", "\"s\"", "null" }) {
        INFO(json);
        CHECK( jkii_find_member(json, strlen(json), "/a", &m) == JKII_ERR_ROOT_TYPE );
    }
    for (const char* json : { "", "{", "{\"a\":1,}", "{x:1}" }) {
        INFO(json);
        CHECK( jkii_find_member(json, strlen(json), "/a", &m) == JKII_ERR_INVALID_INPUT );
    }
}

TEST_CASE("A member's string is copied unescaped") {
    const std::vector<char> d = doc(
            "{\"s\" : \"caf\\u00e9\\n\\\"q\\\"\", \"e\" : \"\", \"z\" : \"a\\u0000b\","
            " \"n\" : 1, \"null\" : null}");
    jkii_member_t m;
    REQUIRE( jkii_find_member(d.data(), d.size(), "/s", &m) == JKII_ERR_OK );
    // Its length and the terminator always suffice.
    std::vector<char> buff(m.value_length + 1, 'x');
    CHECK( jkii_member_string(&m, buff.data(), buff.size()) == JKII_FIELD_ERR_OK );
    CHECK( std::string(buff.data()) == "caf\xc3\xa9\n\"q\"" );
    // Exactly what it takes: 9 bytes, the e with its accent 2 of them, and the
    // terminator.
    char exact[10];
    CHECK( jkii_member_string(&m, exact, sizeof(exact)) == JKII_FIELD_ERR_OK );
    // A byte short: refused, and empty.
    char short_buff[9];
    memset(short_buff, 'x', sizeof(short_buff));
    CHECK( jkii_member_string(&m, short_buff, sizeof(short_buff)) == JKII_FIELD_ERR_COPY );
    CHECK( short_buff[0] == '\0' );
    CHECK( jkii_member_string(&m, nullptr, 0) == JKII_FIELD_ERR_COPY );

    REQUIRE( jkii_find_member(d.data(), d.size(), "/e", &m) == JKII_ERR_OK );
    char one[1] = { 'x' };
    CHECK( jkii_member_string(&m, one, sizeof(one)) == JKII_FIELD_ERR_OK );
    CHECK( one[0] == '\0' );

    // \u0000, which a C string can't hold.
    REQUIRE( jkii_find_member(d.data(), d.size(), "/z", &m) == JKII_ERR_OK );
    char z[8];
    CHECK( jkii_member_string(&m, z, sizeof(z)) == JKII_FIELD_ERR_COPY );

    for (const char* path : { "/n", "/null" }) {
        INFO(path);
        REQUIRE( jkii_find_member(d.data(), d.size(), path, &m) == JKII_ERR_OK );
        char b[8] = "xxxxxxx";
        CHECK( jkii_member_string(&m, b, sizeof(b)) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( b[0] == '\0' );
    }
}

TEST_CASE("A member's number is read as an integer within a range, from its digits") {
    struct Case {
        const char* json;
        int64_t value;
    };
    // Whole, however written.
    const Case whole[] = {
        { "30", 30 }, { "30.0", 30 }, { "3e1", 30 }, { "3E+1", 30 }, { "300e-1", 30 },
        { "0.3e2", 30 }, { "30.000e0", 30 }, { "1.5e1", 15 }, { "-30.0", -30 },
        { "0", 0 }, { "-0", 0 }, { "-0.0", 0 }, { "0e-5", 0 }, { "0e100000", 0 },
        { "0.000e100000", 0 }, { "1e18", INT64_C(1000000000000000000) },
        { "9223372036854775807", INT64_MAX }, { "-9223372036854775808", INT64_MIN },
        { "9.223372036854775807e18", INT64_MAX }, { "-9.223372036854775808e18", INT64_MIN },
        { "9007199254740993.0", INT64_C(9007199254740993) },
    };
    for (const Case& c : whole) {
        INFO(c.json);
        jkii_member_t m = number(c.json);
        int64_t v = 12345;
        CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_OK );
        CHECK( v == c.value );
    }

    // Not whole: a fraction, written so that a double couldn't tell, as
    // 2^52 + 1.5 rounds to a whole double.
    for (const char* json : { "30.5", "1e-1", "0.5", "-0.25", "5e-100000",
            "4503599627370497.5", "9007199254740993.5", "1.00000000000000000001" }) {
        INFO(json);
        jkii_member_t m = number(json);
        int64_t v = 12345;
        CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( v == 12345 );
    }

    // Past int64_t.
    for (const char* json : { "9223372036854775808", "1e19", "9.3e18", "1e100000" }) {
        INFO(json);
        jkii_member_t m = number(json);
        int64_t v = 0;
        CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_NUM_OVERFLOW );
    }
    for (const char* json : { "-9223372036854775809", "-1e19", "-1e100000" }) {
        INFO(json);
        jkii_member_t m = number(json);
        int64_t v = 0;
        CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_NUM_UNDERFLOW );
    }

    // The range given.
    jkii_member_t m = number("30.0");
    int64_t v = 0;
    CHECK( jkii_member_integer(&m, 0, 29, &v) == JKII_FIELD_ERR_NUM_OVERFLOW );
    CHECK( jkii_member_integer(&m, 31, 100, &v) == JKII_FIELD_ERR_NUM_UNDERFLOW );
    CHECK( jkii_member_integer(&m, 30, 30, &v) == JKII_FIELD_ERR_OK );
    CHECK( v == 30 );

    // Not a number: a string of one included, which is the caller's to read.
    for (const char* json : { "\"30\"", "null", "true", "[30]", "{\"a\":30}" }) {
        INFO(json);
        jkii_member_t not_number = number(json);
        int64_t w = 7;
        CHECK( jkii_member_integer(&not_number, INT64_MIN, INT64_MAX, &w) ==
                JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( w == 7 );
    }
}

TEST_CASE("A member found by its path is read as it is written") {
    const std::vector<char> d = doc(
            "{\"config\" : {\"timeout\" : 30.0, \"name\" : \"d\\u00e9v\", \"n\" : [1, 2.5]}}");
    jkii_member_t m;
    int64_t timeout = 0;
    REQUIRE( jkii_find_member(d.data(), d.size(), "/config/timeout", &m) == JKII_ERR_OK );
    CHECK( jkii_member_integer(&m, 1, 3600, &timeout) == JKII_FIELD_ERR_OK );
    CHECK( timeout == 30 );
    char name[16];
    REQUIRE( jkii_find_member(d.data(), d.size(), "/config/name", &m) == JKII_ERR_OK );
    CHECK( jkii_member_string(&m, name, sizeof(name)) == JKII_FIELD_ERR_OK );
    CHECK( std::string(name) == "d\xc3\xa9v" );
    REQUIRE( jkii_find_member(d.data(), d.size(), "/config/n/[1]", &m) == JKII_ERR_OK );
    CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &timeout) == JKII_FIELD_ERR_TYPE_MISMATCH );
}

TEST_CASE("A member's number is read as a double, of any form") {
    struct Case {
        const char* json;
        double value;
    };
    const Case numbers[] = {
        { "30", 30.0 }, { "-2.5", -2.5 }, { "3e1", 30.0 }, { "0.25", 0.25 },
        { "1e308", 1e308 }, { "-0", -0.0 },
        // An integer a double can't hold: the nearest one.
        { "9007199254740993", 9007199254740992.0 },
        { "123456789012345678901234567890", 1.2345678901234568e29 },
    };
    for (const Case& c : numbers) {
        INFO(c.json);
        jkii_member_t m = number(c.json);
        double v = 7;
        CHECK( jkii_member_double(&m, &v) == JKII_FIELD_ERR_OK );
        CHECK( v == c.value );
    }

    // Past a double's range, as jkii_parse() reports it: never an infinity.
    for (const char* json : { "1e400", "-1e400" }) {
        INFO(json);
        jkii_member_t m = number(json);
        double v = 7;
        CHECK( jkii_member_double(&m, &v) == JKII_FIELD_ERR_NUM_OVERFLOW );
        CHECK( v == 7 );
    }
    jkii_member_t tiny = number("1e-400");
    double v = 7;
    CHECK( jkii_member_double(&tiny, &v) == JKII_FIELD_ERR_NUM_UNDERFLOW );
    CHECK( v == 7 );

    for (const char* json : { "\"30\"", "null", "true", "[1]", "{}" }) {
        INFO(json);
        jkii_member_t m = number(json);
        double w = 7;
        CHECK( jkii_member_double(&m, &w) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( w == 7 );
    }
}

TEST_CASE("A member's true or false is read as a boolean") {
    jkii_member_t m = number("true");
    jkii_boolean_t b = JKII_FALSE;
    CHECK( jkii_member_boolean(&m, &b) == JKII_FIELD_ERR_OK );
    CHECK( b == JKII_TRUE );
    m = number("false");
    CHECK( jkii_member_boolean(&m, &b) == JKII_FIELD_ERR_OK );
    CHECK( b == JKII_FALSE );

    // Anything else, "true" as a string included: b as it was.
    for (const char* json : { "\"true\"", "null", "1", "0", "[true]" }) {
        INFO(json);
        jkii_member_t other = number(json);
        jkii_boolean_t c = JKII_TRUE;
        CHECK( jkii_member_boolean(&other, &c) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( c == JKII_TRUE );
    }

    // Found by its path.
    const std::vector<char> d = doc("{\"on\" : false}");
    REQUIRE( jkii_find_member(d.data(), d.size(), "/on", &m) == JKII_ERR_OK );
    b = JKII_TRUE;
    CHECK( jkii_member_boolean(&m, &b) == JKII_FIELD_ERR_OK );
    CHECK( b == JKII_FALSE );
}

TEST_CASE("A string holding exactly a number, true or false is read as it, when asked") {
    struct IntegerCase {
        const char* json;
        int64_t value;
    };
    const IntegerCase integers[] = {
        { R"("30")", 30 }, { R"("-0")", 0 }, { R"("3e1")", 30 }, { R"("30.0")", 30 },
        { R"("300e-1")", 30 }, { R"("-2E+1")", -20 },
        { R"("9223372036854775807")", INT64_MAX },
    };
    for (const IntegerCase& c : integers) {
        INFO(c.json);
        const jkii_member_t m = number(c.json);
        jkii_member_t u;
        int64_t v = 7;
        // Not unless asked.
        CHECK( jkii_member_integer(&m, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_TYPE_MISMATCH );
        const jkii_member_t* read = jkii_member_unquote(&m, &u);
        CHECK( read == &u );
        CHECK( jkii_member_integer(read, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_OK );
        CHECK( v == c.value );
        CHECK( u.value == m.value );
        CHECK( u.value_length == m.value_length );
    }

    jkii_member_t u;
    double d = 0;
    jkii_member_t m = number(R"("-2.5")");
    CHECK( jkii_member_unquote(&m, &u)->type == JKII_FIELD_TYPE_DOUBLE );
    CHECK( jkii_member_double(jkii_member_unquote(&m, &u), &d) == JKII_FIELD_ERR_OK );
    CHECK( d == -2.5 );

    jkii_boolean_t b = JKII_FALSE;
    m = number(R"("true")");
    CHECK( jkii_member_boolean(&m, &b) == JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( jkii_member_boolean(jkii_member_unquote(&m, &u), &b) == JKII_FIELD_ERR_OK );
    CHECK( b == JKII_TRUE );
    m = number(R"("false")");
    CHECK( jkii_member_boolean(jkii_member_unquote(&m, &u), &b) == JKII_FIELD_ERR_OK );
    CHECK( b == JKII_FALSE );

    // The member's name stays.
    const std::vector<char> obj = doc(R"({"timeout" : "30"})");
    jkii_member_iter_t iter;
    memset(&iter, 0, sizeof(iter));
    REQUIRE( jkii_next_member(obj.data(), obj.size(), &iter, &m) == JKII_ERR_OK );
    const jkii_member_t* read = jkii_member_unquote(&m, &u);
    REQUIRE( read == &u );
    CHECK( std::string(u.key, u.key_length) == "timeout" );
    CHECK( u.type == JKII_FIELD_TYPE_INTEGER );
}

TEST_CASE("A string taken as a number reads as the number would, range and all") {
    jkii_member_t u;
    int64_t v = 7;
    double d = 7;

    jkii_member_t m = number(R"("30.5")");
    CHECK( jkii_member_integer(jkii_member_unquote(&m, &u), INT64_MIN, INT64_MAX, &v) ==
            JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( jkii_member_double(jkii_member_unquote(&m, &u), &d) == JKII_FIELD_ERR_OK );
    CHECK( d == 30.5 );

    m = number(R"("5000")");
    CHECK( jkii_member_integer(jkii_member_unquote(&m, &u), 1, 3600, &v) ==
            JKII_FIELD_ERR_NUM_OVERFLOW );
    m = number(R"("0")");
    CHECK( jkii_member_integer(jkii_member_unquote(&m, &u), 1, 3600, &v) ==
            JKII_FIELD_ERR_NUM_UNDERFLOW );
    m = number(R"("9223372036854775808")");
    CHECK( jkii_member_integer(jkii_member_unquote(&m, &u), INT64_MIN, INT64_MAX, &v) ==
            JKII_FIELD_ERR_NUM_OVERFLOW );
    m = number(R"("-1e400")");
    CHECK( jkii_member_double(jkii_member_unquote(&m, &u), &d) == JKII_FIELD_ERR_NUM_OVERFLOW );
    m = number(R"("1e-400")");
    CHECK( jkii_member_double(jkii_member_unquote(&m, &u), &d) == JKII_FIELD_ERR_NUM_UNDERFLOW );
    CHECK( v == 7 );
    CHECK( d == 30.5 );

    // A number longer than jkii reads, as the number would be.
    const std::string longer = "\"1." + std::string(400, '0') + "\"";
    m = number(longer.c_str());
    CHECK( jkii_member_unquote(&m, &u)->type == JKII_FIELD_TYPE_ANY );
    CHECK( jkii_member_double(jkii_member_unquote(&m, &u), &d) == JKII_FIELD_ERR_TYPE_MISMATCH );
}

TEST_CASE("A string that is anything but exactly a number, true or false stays a string") {
    for (const char* json : {
            R"(" 30")", R"("30 ")", R"("+30")", R"("030")", R"("-030")", R"(".5")", R"("5.")",
            R"("1e")", R"("1e+")", R"("-")", R"("0x1E")", R"("1_000")", R"("NaN")",
            R"("Infinity")", R"("-Infinity")", R"("TRUE")", R"("True")", R"("true ")",
            R"("null")", R"("")", R"("30\n")", R"("\u0033\u0030")", R"("3\u0030")",
            R"("\u0074rue")", R"("30\u0000")" }) {
        INFO(json);
        const jkii_member_t m = number(json);
        jkii_member_t u;
        memset(&u, 0, sizeof(u));
        const jkii_member_t* read = jkii_member_unquote(&m, &u);
        CHECK( read == &m );
        CHECK( u.value == nullptr );
        int64_t v = 7;
        double d = 7;
        jkii_boolean_t b = JKII_TRUE;
        CHECK( jkii_member_integer(read, INT64_MIN, INT64_MAX, &v) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( jkii_member_double(read, &d) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( jkii_member_boolean(read, &b) == JKII_FIELD_ERR_TYPE_MISMATCH );
        CHECK( v == 7 );
        CHECK( d == 7 );
        CHECK( b == JKII_TRUE );
    }

    // What isn't a string is itself, read as before.
    for (const char* json : { "30", "true", "null", "[\"30\"]", "{\"a\":\"30\"}" }) {
        INFO(json);
        const jkii_member_t m = number(json);
        jkii_member_t u;
        CHECK( jkii_member_unquote(&m, &u) == &m );
    }
}
