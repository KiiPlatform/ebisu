#include "catch_amalgamated.hpp"
#include <jkii.h>
#include <jkii_utils.h>
#include <jkii_writer.h>
#include <locale.h>
#include <string.h>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace {

typedef std::function<void(jkii_writer_t*)> Writes;

struct Written {
    jkii_write_err_t err;
    std::string json;
    size_t len;
};

// What writes produce into a buffer of buff_size.
Written into_buffer(const Writes& writes, size_t buff_size = 256)
{
    std::vector<char> buff(buff_size);
    jkii_writer_t w;
    jkii_writer_init_buff(&w, buff.data(), buff.size());
    writes(&w);
    Written written;
    written.len = 0;
    written.err = jkii_writer_finish(&w, &written.len);
    written.json = buff.data();
    return written;
}

// As above, through a callback with a buffer of buff_size, and how many times
// it was called.
Written through_callback(const Writes& writes, size_t buff_size, int* calls = nullptr)
{
    struct Out {
        std::string json;
        int calls;
        size_t largest;
    } out = { "", 0, 0 };
    std::vector<char> buff(buff_size);
    jkii_writer_t w;
    jkii_writer_init_cb(&w, buff.data(), buff.size(),
            [](const char* data, size_t size, void* userdata) {
                Out* o = (Out*)userdata;
                o->json.append(data, size);
                ++o->calls;
                o->largest = size > o->largest ? size : o->largest;
                return size;
            }, &out);
    writes(&w);
    Written written;
    written.len = 0;
    written.err = jkii_writer_finish(&w, &written.len);
    written.json = out.json;
    CHECK( out.largest <= buff_size );
    if (calls != nullptr) {
        *calls = out.calls;
    }
    return written;
}

std::string json_of(const Writes& writes)
{
    Written written = into_buffer(writes);
    REQUIRE( written.err == JKII_WRITE_OK );
    REQUIRE( written.len == written.json.size() );
    return written.json;
}

// A device's state, with a member of every kind.
void state(jkii_writer_t* w)
{
    jkii_write_object_start(w);
    jkii_write_key(w, "power");
    jkii_write_bool(w, true);
    jkii_write_key(w, "presetTemperature");
    jkii_write_int(w, -5);
    jkii_write_key(w, "humidity");
    jkii_write_double(w, 45.25, 1);
    jkii_write_key(w, "modes");
    jkii_write_array_start(w);
    jkii_write_string(w, "cool");
    jkii_write_string(w, "dry");
    jkii_write_object_start(w);
    jkii_write_object_end(w);
    jkii_write_array_start(w);
    jkii_write_array_end(w);
    jkii_write_array_end(w);
    jkii_write_key(w, "note");
    jkii_write_null(w);
    jkii_write_key(w, "raw");
    jkii_write_raw(w, "{\"a\":[1,2]}", 11);
    jkii_write_object_end(w);
}

const std::string STATE =
    "{\"power\":true,\"presetTemperature\":-5,\"humidity\":45.3,"
    "\"modes\":[\"cool\",\"dry\",{},[]],\"note\":null,\"raw\":{\"a\":[1,2]}}";

}

TEST_CASE( "The JSON writer puts in the commas, colons and quotes" ) {
    CHECK( json_of(state) == STATE );

    // What it wrote parses.
    std::string json = json_of(state);
    char mode[16];
    jkii_field_t fields[3];
    memset(fields, 0, sizeof(fields));
    fields[0].path = "/modes/[1]";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = mode;
    fields[0].field_copy_buff_size = sizeof(mode);
    fields[1].name = "presetTemperature";
    fields[1].type = JKII_FIELD_TYPE_INTEGER;
    CHECK( jkii_parse(json.c_str(), json.size(), fields) == JKII_ERR_OK );
    CHECK( std::string(mode) == "dry" );
    CHECK( fields[1].field_copy.int_value == -5 );
}

TEST_CASE( "The JSON writer writes a value of any kind at the top" ) {
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_uint(w, 42); }) == "42" );
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_string(w, "x"); }) == "\"x\"" );
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_null(w); }) == "null" );
    CHECK( json_of([](jkii_writer_t* w) {
        jkii_write_array_start(w);
        jkii_write_array_end(w);
    }) == "[]" );
}

TEST_CASE( "The JSON writer escapes strings and keys as jkii_escape_str does" ) {
    const char text[] = "a\"b\\c/d\b\f\n\r\t\x01\x1f caf\xc3\xa9";
    char escaped[128];
    REQUIRE( jkii_escape_str(text, escaped, sizeof(escaped)) >= 0 );
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_string(w, text); })
            == "\"" + std::string(escaped) + "\"" );
    CHECK( json_of([&](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key(w, text);
        jkii_write_int(w, 1);
        jkii_write_object_end(w);
    }) == "{\"" + std::string(escaped) + "\":1}" );

    // With a length, null bytes too.
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_string_n(w, "a\0b", 3); })
            == "\"a\\u0000b\"" );
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_string_n(w, nullptr, 0); }) == "\"\"" );
}

TEST_CASE( "The JSON writer writes a key of a given length" ) {
    CHECK( json_of([](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key_n(w, "a\0b", 3);
        jkii_write_int(w, 1);
        jkii_write_key_n(w, "cd", 1);
        jkii_write_int(w, 2);
        jkii_write_key_n(w, nullptr, 0);
        jkii_write_int(w, 3);
        jkii_write_object_end(w);
    }) == "{\"a\\u0000b\":1,\"c\":2,\"\":3}" );
    CHECK( into_buffer([](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key_n(w, nullptr, 1);
    }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) {
        jkii_write_array_start(w);
        jkii_write_key_n(w, "k", 1);
    }).err == JKII_WRITE_ERR_STRUCTURE );
}

namespace {

// The members of an object, or the elements of an array, written back as jkii
// read them.
Writes copy_of(const std::string& json)
{
    return [json](jkii_writer_t* w) {
        jkii_member_t root;
        REQUIRE( jkii_root_value(json.data(), json.size(), &root) == JKII_ERR_OK );
        bool object = root.type == JKII_FIELD_TYPE_OBJECT;
        if (object) {
            jkii_write_object_start(w);
        } else {
            jkii_write_array_start(w);
        }
        jkii_member_iter_t it = { 0, 0 };
        jkii_member_t m;
        jkii_parse_err_t res;
        while ((res = jkii_next_member(json.data(), json.size(), &it, &m)) == JKII_ERR_OK) {
            if (object) {
                jkii_write_member_key(w, &m);
            }
            jkii_write_member_value(w, &m);
        }
        CHECK( res == JKII_ERR_NOT_FOUND );
        if (object) {
            jkii_write_object_end(w);
        } else {
            jkii_write_array_end(w);
        }
    };
}

std::string string_at(const std::string& json, const char* path)
{
    jkii_member_t m;
    char s[64];
    REQUIRE( jkii_find_member(json.data(), json.size(), path, &m) == JKII_ERR_OK );
    REQUIRE( jkii_member_string(&m, s, sizeof(s)) == JKII_FIELD_ERR_OK );
    return s;
}

}

TEST_CASE( "The JSON writer writes members jkii read as they were written" ) {
    // The whitespace between members goes, as the writer puts in its own
    // commas and colons; inside a value it stays, as written.
    const std::string doc =
        "{ \"a\\u0000b\" : \"x\\\"y\\u00e9 \\/\",\n"
        "  \"n\": -1.5e3, \"o\": { \"k\" : [1, 2] }, \"t\": true,"
        " \"z\": null, \"e\": \"\", \"big\": 123456789012345678901234567890 }";
    const std::string copy = json_of(copy_of(doc));
    CHECK( copy ==
        "{\"a\\u0000b\":\"x\\\"y\\u00e9 \\/\",\"n\":-1.5e3,"
        "\"o\":{ \"k\" : [1, 2] },\"t\":true,\"z\":null,\"e\":\"\","
        "\"big\":123456789012345678901234567890}" );
    CHECK( json_of(copy_of("[\"s\", 1, [ ], {}]")) == "[\"s\",1,[ ],{}]" );

    // It reads as the document did.
    for (const char* path : { "/a\\u0000b", "/e" }) {
        INFO( path );
        CHECK( string_at(copy, path) == string_at(doc, path) );
    }
    jkii_member_t m;
    int64_t k = 0;
    REQUIRE( jkii_find_member(copy.data(), copy.size(), "/o/k/[1]", &m) == JKII_ERR_OK );
    CHECK( jkii_member_integer(&m, 0, 10, &k) == JKII_FIELD_ERR_OK );
    CHECK( k == 2 );
}

TEST_CASE( "The JSON writer copies a string of any length, a small buffer at a time" ) {
    // Unescaped, the key would be a C string no longer, with its null bytes.
    std::string text;
    for (int i = 0; i < 1000; ++i) {
        text += "ab\\n\\u0000\\u00e9";
    }
    const std::string doc = "{\"" + text + "\":\"" + text + "\"}";
    Written written = through_callback(copy_of(doc), 8);
    CHECK( written.err == JKII_WRITE_OK );
    CHECK( written.json == doc );
    CHECK( written.len == doc.size() );
}

TEST_CASE( "The JSON writer writes a value found, a root one, or one unquoted" ) {
    const std::string doc = "{\"a\":{\"s\":\"q\\\"\",\"n\":\"30\"}}";
    jkii_member_t m;
    REQUIRE( jkii_find_member(doc.data(), doc.size(), "/a/s", &m) == JKII_ERR_OK );
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_member_value(w, &m); }) == "\"q\\\"\"" );
    REQUIRE( jkii_find_member(doc.data(), doc.size(), "/a", &m) == JKII_ERR_OK );
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_member_value(w, &m); })
            == "{\"s\":\"q\\\"\",\"n\":\"30\"}" );

    const std::string root = "\"r\\u00e9\"";
    REQUIRE( jkii_root_value(root.data(), root.size(), &m) == JKII_ERR_OK );
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_member_value(w, &m); }) == root );

    // A number in a string, as the string, or as the number it holds.
    jkii_member_t n, unquoted;
    REQUIRE( jkii_find_member(doc.data(), doc.size(), "/a/n", &n) == JKII_ERR_OK );
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_member_value(w, &n); }) == "\"30\"" );
    CHECK( json_of([&](jkii_writer_t* w) {
        jkii_write_member_value(w, jkii_member_unquote(&n, &unquoted));
    }) == "30" );

    // An empty string needs no characters.
    jkii_member_t empty;
    memset(&empty, 0, sizeof(empty));
    empty.type = JKII_FIELD_TYPE_STRING;
    CHECK( json_of([&](jkii_writer_t* w) { jkii_write_member_value(w, &empty); }) == "\"\"" );
}

TEST_CASE( "The JSON writer refuses a member it can't write, or where it can't go" ) {
    const std::string array = "[1]";
    jkii_member_iter_t it = { 0, 0 };
    jkii_member_t element;
    REQUIRE( jkii_next_member(array.data(), array.size(), &it, &element) == JKII_ERR_OK );
    const std::string object = "{\"k\":1}";
    jkii_member_iter_t it2 = { 0, 0 };
    jkii_member_t member;
    REQUIRE( jkii_next_member(object.data(), object.size(), &it2, &member) == JKII_ERR_OK );

    // An array's element has no key.
    CHECK( into_buffer([&](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_member_key(w, &element);
    }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_member_key(w, nullptr);
    }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_member_value(w, nullptr); }).err
            == JKII_WRITE_ERR_VALUE );
    // A number of no length, and characters that aren't there.
    jkii_member_t no_length = element;
    no_length.value_length = 0;
    CHECK( into_buffer([&](jkii_writer_t* w) { jkii_write_member_value(w, &no_length); }).err
            == JKII_WRITE_ERR_VALUE );
    jkii_member_t no_value = element;
    no_value.type = JKII_FIELD_TYPE_STRING;
    no_value.value = nullptr;
    CHECK( into_buffer([&](jkii_writer_t* w) { jkii_write_member_value(w, &no_value); }).err
            == JKII_WRITE_ERR_VALUE );

    CHECK( into_buffer([&](jkii_writer_t* w) { jkii_write_member_key(w, &member); }).err
            == JKII_WRITE_ERR_STRUCTURE );
    CHECK( into_buffer([&](jkii_writer_t* w) {
        jkii_write_array_start(w);
        jkii_write_member_key(w, &member);
    }).err == JKII_WRITE_ERR_STRUCTURE );
    CHECK( into_buffer([&](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_member_key(w, &member);
        jkii_write_member_key(w, &member);
    }).err == JKII_WRITE_ERR_STRUCTURE );
    CHECK( into_buffer([&](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_member_value(w, &member);
    }).err == JKII_WRITE_ERR_STRUCTURE );
    // Two members, with a comma between.
    CHECK( json_of([&](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_member_key(w, &member);
        jkii_write_member_value(w, &member);
        jkii_write_member_key(w, &member);
        jkii_write_member_value(w, &element);
        jkii_write_object_end(w);
    }) == "{\"k\":1,\"k\":1}" );
}

TEST_CASE( "The JSON writer writes integers whole" ) {
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_int(w, 0); }) == "0" );
    CHECK( json_of([](jkii_writer_t* w) {
        jkii_write_int(w, std::numeric_limits<int64_t>::min());
    }) == "-9223372036854775808" );
    CHECK( json_of([](jkii_writer_t* w) {
        jkii_write_int(w, std::numeric_limits<int64_t>::max());
    }) == "9223372036854775807" );
    CHECK( json_of([](jkii_writer_t* w) {
        jkii_write_uint(w, std::numeric_limits<uint64_t>::max());
    }) == "18446744073709551615" );
}

TEST_CASE( "The JSON writer writes doubles with their decimals, whatever the locale" ) {
    struct Case {
        double v;
        int decimals;
        const char* json;
    };
    const Case cases[] = {
        { 1.5, 2, "1.50" },
        { 21.25, 1, "21.3" },
        { -21.25, 1, "-21.3" },
        { 0.05, 1, "0.1" },
        { 2.675, 2, "2.67" }, // just below 2.675 in binary
        { 123.0, 0, "123" },
        { -0.004, 2, "0.00" }, // no sign on what rounds to 0
        { -0.0, 0, "0" },
        { 0.000000001, 9, "0.000000001" },
        { 18446744073709549568.0, 0, "18446744073709549568" }, // the largest below 2^64
        { 1e19, 0, "10000000000000000000" },
        { 1e10, 9, "10000000000.000000000" },
        { 0.5, 0, "1" },
        { -2.5, 0, "-3" }, // half away from zero
    };
    for (const Case& c : cases) {
        INFO( c.json );
        CHECK( json_of([&](jkii_writer_t* w) { jkii_write_double(w, c.v, c.decimals); }) == c.json );
    }

    // A comma for a point where the locale has one, which printf would use.
    if (setlocale(LC_NUMERIC, "de_DE.UTF-8") != nullptr || setlocale(LC_NUMERIC, "de_DE") != nullptr) {
        CHECK( json_of([](jkii_writer_t* w) { jkii_write_double(w, 1.5, 1); }) == "1.5" );
        setlocale(LC_NUMERIC, "C");
    } else {
        WARN( "no German locale here: the locale isn't changed" );
    }
}

TEST_CASE( "The JSON writer refuses a double JSON can't hold" ) {
    const double values[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
    };
    for (double v : values) {
        CHECK( into_buffer([&](jkii_writer_t* w) { jkii_write_double(w, v, 2); }).err == JKII_WRITE_ERR_VALUE );
    }
    // Too large for its decimals, or too many of them.
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_double(w, 2e19, 0); }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_double(w, 2e10, 9); }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_double(w, 1.0, 10); }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_double(w, 1.0, -1); }).err == JKII_WRITE_ERR_VALUE );
}

TEST_CASE( "The JSON writer never truncates, and says how much it needed" ) {
    const size_t len = STATE.size();
    SECTION("a buffer one byte short") {
        Written written = into_buffer(state, len);
        CHECK( written.err == JKII_WRITE_ERR_TOO_LARGE );
        CHECK( written.len == len );
    }
    SECTION("a buffer just large enough, with the terminator") {
        Written written = into_buffer(state, len + 1);
        CHECK( written.err == JKII_WRITE_OK );
        CHECK( written.json == STATE );
    }
    SECTION("a dry run") {
        jkii_writer_t w;
        jkii_writer_init_buff(&w, nullptr, 0);
        state(&w);
        size_t needed = 0;
        CHECK( jkii_writer_finish(&w, &needed) == JKII_WRITE_OK );
        CHECK( needed == len );
    }
    SECTION("a call after the buffer ran out") {
        std::vector<char> buff(4);
        jkii_writer_t w;
        jkii_writer_init_buff(&w, buff.data(), buff.size());
        CHECK( jkii_write_array_start(&w) == JKII_WRITE_OK );
        CHECK( jkii_write_string(&w, "long") == JKII_WRITE_ERR_TOO_LARGE );
        CHECK( jkii_write_array_end(&w) == JKII_WRITE_ERR_TOO_LARGE );
        size_t needed = 0;
        CHECK( jkii_writer_finish(&w, &needed) == JKII_WRITE_ERR_TOO_LARGE );
        CHECK( needed == strlen("[\"long\"]") );
        CHECK( strlen(buff.data()) < buff.size() );
    }
}

TEST_CASE( "The JSON writer refuses what wouldn't be JSON" ) {
    struct Case {
        const char* what;
        Writes writes;
    };
    const Case cases[] = {
        { "a value where a key is expected", [](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_int(w, 1);
        } },
        { "a key in an array", [](jkii_writer_t* w) {
            jkii_write_array_start(w);
            jkii_write_key(w, "k");
        } },
        { "a key at the top", [](jkii_writer_t* w) { jkii_write_key(w, "k"); } },
        { "two keys in a row", [](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key(w, "a");
            jkii_write_key(w, "b");
        } },
        { "an object ended after its key", [](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key(w, "a");
            jkii_write_object_end(w);
        } },
        { "an array ended as an object", [](jkii_writer_t* w) {
            jkii_write_array_start(w);
            jkii_write_object_end(w);
        } },
        { "an end with nothing open", [](jkii_writer_t* w) { jkii_write_array_end(w); } },
        { "a second value at the top", [](jkii_writer_t* w) {
            jkii_write_int(w, 1);
            jkii_write_int(w, 2);
        } },
        { "a container left open", [](jkii_writer_t* w) { jkii_write_object_start(w); } },
        { "nothing", [](jkii_writer_t*) {} },
    };
    for (const Case& c : cases) {
        INFO( c.what );
        CHECK( into_buffer(c.writes).err == JKII_WRITE_ERR_STRUCTURE );
    }
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_string(w, nullptr); }).err == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_raw(w, "", 0); }).err == JKII_WRITE_ERR_VALUE );
}

// JSON text is UTF-8, as RFC 8259 has it for text exchanged, and jkii reads
// none that isn't. The writer passed any other byte through, and said
// JKII_WRITE_OK: Latin-1, or a name cut in the middle of a character.
TEST_CASE( "The JSON writer refuses a string or key that isn't UTF-8" ) {
    const char* const not_utf8[] = {
        "caf\xe9",          // Latin-1
        "caf\xc3",          // cut before its last byte
        "\x80",             // a continuation byte alone
        "\xc0\xaf",         // overlong
        "\xe0\x80\xaf",     // overlong
        "\xed\xa0\x80",     // a surrogate, U+D800
        "\xf4\x90\x80\x80", // past U+10FFFF
        "\xf5\x80\x80\x80", // no such lead byte
        "\xe2\x82",         // cut
    };
    for (const char* s : not_utf8) {
        INFO( "string " << (const void*)s );
        CHECK( into_buffer([s](jkii_writer_t* w) { jkii_write_string(w, s); }).err
                == JKII_WRITE_ERR_VALUE );
        CHECK( into_buffer([s](jkii_writer_t* w) { jkii_write_string_n(w, s, strlen(s)); }).err
                == JKII_WRITE_ERR_VALUE );
        CHECK( into_buffer([s](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key(w, s);
            jkii_write_int(w, 1);
            jkii_write_object_end(w);
        }).err == JKII_WRITE_ERR_VALUE );
        CHECK( into_buffer([s](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key_n(w, s, strlen(s));
            jkii_write_int(w, 1);
            jkii_write_object_end(w);
        }).err == JKII_WRITE_ERR_VALUE );
    }

    // Each kind of sequence, at its limits, is written, and read back.
    const char* const utf8[] = {
        "\x7f", "\xc2\x80", "\xdf\xbf", "\xe0\xa0\x80", "\xed\x9f\xbf",
        "\xee\x80\x80", "\xef\xbf\xbf", "\xf0\x90\x80\x80", "\xf4\x8f\xbf\xbf",
        "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80",
    };
    for (const char* s : utf8) {
        INFO( "string " << (const void*)s );
        Written written = into_buffer([s](jkii_writer_t* w) {
            jkii_write_array_start(w);
            jkii_write_string(w, s);
            jkii_write_array_end(w);
        });
        CHECK( written.err == JKII_WRITE_OK );
        jkii_member_t read;
        CHECK( jkii_root_value(written.json.c_str(), written.json.size(), &read) == JKII_ERR_OK );
    }
}

TEST_CASE( "The JSON writer takes bytes that aren't UTF-8 as Latin-1 when asked" ) {
    struct Case {
        std::string bytes;
        std::string json;
    };
    const Case cases[] = {
        { "plain", "plain" },
        { "caf\xe9", "caf\xc3\xa9" },                                   // Latin-1 é
        { "\x80", "\xc2\x80" },
        { "\xff", "\xc3\xbf" },
        { "caf\xc3", "caf\xc3\x83" },                                   // cut before its last byte
        { "\xe2\x82", "\xc3\xa2\xc2\x82" },                             // cut
        { "\xc0\xaf", "\xc3\x80\xc2\xaf" },                             // overlong
        { "\xed\xa0\x80", "\xc3\xad\xc2\xa0\xc2\x80" },                 // a surrogate, U+D800
        { "\xf4\x90\x80\x80", "\xc3\xb4\xc2\x90\xc2\x80\xc2\x80" },     // past U+10FFFF
        { "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80",
          "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80" },                // UTF-8 already
        { "\xe9\xc3\xa9\xe9", "\xc3\xa9\xc3\xa9\xc3\xa9" },              // both, side by side
        { std::string("a\0\xe9\n\"\x7f", 6), "a\\u0000\xc3\xa9\\n\\\"\x7f" }, // escaped as ever
    };
    for (const Case& c : cases) {
        INFO( "bytes " << c.json );
        const std::string bytes = c.bytes;
        Written string = into_buffer([&bytes](jkii_writer_t* w) {
            jkii_write_array_start(w);
            jkii_write_string_latin1_n(w, bytes.data(), bytes.size());
            jkii_write_array_end(w);
        });
        CHECK( string.err == JKII_WRITE_OK );
        CHECK( string.json == "[\"" + c.json + "\"]" );
        CHECK( string.len == string.json.size() );
        jkii_member_t read;
        CHECK( jkii_root_value(string.json.c_str(), string.json.size(), &read) == JKII_ERR_OK );

        Written key = into_buffer([&bytes](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key_latin1_n(w, bytes.data(), bytes.size());
            jkii_write_int(w, 1);
            jkii_write_object_end(w);
        });
        CHECK( key.err == JKII_WRITE_OK );
        CHECK( key.json == "{\"" + c.json + "\":1}" );

        // A byte at a time through a callback, as into a buffer.
        Written small = through_callback([&bytes](jkii_writer_t* w) {
            jkii_write_string_latin1_n(w, bytes.data(), bytes.size());
        }, 1);
        CHECK( small.err == JKII_WRITE_OK );
        CHECK( small.json == "\"" + c.json + "\"" );
    }

    // Nothing, from NULL, is the empty string; NULL with a length is refused.
    CHECK( json_of([](jkii_writer_t* w) { jkii_write_string_latin1_n(w, nullptr, 0); }) == "\"\"" );
    CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_string_latin1_n(w, nullptr, 1); }).err
            == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key_latin1_n(w, nullptr, 1);
    }).err == JKII_WRITE_ERR_VALUE );

    // Where the writer counts for jkii_writer_finish(), the bytes written count.
    Written too_large = into_buffer([](jkii_writer_t* w) { jkii_write_string_latin1_n(w, "\xe9\xe9", 2); }, 4);
    CHECK( too_large.err == JKII_WRITE_ERR_TOO_LARGE );
    CHECK( too_large.len == 6 );

    // Its structure is checked as for any string.
    CHECK( into_buffer([](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_string_latin1_n(w, "v", 1);
    }).err == JKII_WRITE_ERR_STRUCTURE );
}

TEST_CASE( "A sequence cut by the length is cut, though it goes on past it" ) {
    // The bytes after the length make the sequence whole: only a writer that
    // stops at the length sees it cut. A literal's NUL would cut it anyway.
    const char bytes[] = "caf\xc3\xa9";
    CHECK( json_of([&bytes](jkii_writer_t* w) { jkii_write_string_latin1_n(w, bytes, 4); })
            == "\"caf\xc3\x83\"" );
    CHECK( json_of([&bytes](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key_latin1_n(w, bytes, 4);
        jkii_write_int(w, 1);
        jkii_write_object_end(w);
    }) == "{\"caf\xc3\x83\":1}" );
    CHECK( into_buffer([&bytes](jkii_writer_t* w) { jkii_write_string_n(w, bytes, 4); }).err
            == JKII_WRITE_ERR_VALUE );
    CHECK( into_buffer([&bytes](jkii_writer_t* w) {
        jkii_write_object_start(w);
        jkii_write_key_n(w, bytes, 4);
        jkii_write_int(w, 1);
        jkii_write_object_end(w);
    }).err == JKII_WRITE_ERR_VALUE );
}

TEST_CASE( "A JSON writer's error sticks" ) {
    std::vector<char> buff(64);
    jkii_writer_t w;
    jkii_writer_init_buff(&w, buff.data(), buff.size());
    CHECK( jkii_write_array_start(&w) == JKII_WRITE_OK );
    CHECK( jkii_write_key(&w, "k") == JKII_WRITE_ERR_STRUCTURE );
    const std::string before = buff.data();
    CHECK( jkii_write_array_end(&w) == JKII_WRITE_ERR_STRUCTURE );
    CHECK( jkii_write_int(&w, 1) == JKII_WRITE_ERR_STRUCTURE );
    CHECK( jkii_write_double(&w, 1e300, 0) == JKII_WRITE_ERR_STRUCTURE );
    CHECK( jkii_writer_finish(&w, nullptr) == JKII_WRITE_ERR_STRUCTURE );
    CHECK( std::string(buff.data()) == before );
}

TEST_CASE( "The JSON writer nests as deep as JKII_WRITER_MAX_DEPTH" ) {
    std::string expected = std::string(JKII_WRITER_MAX_DEPTH, '[') + std::string(JKII_WRITER_MAX_DEPTH, ']');
    CHECK( json_of([](jkii_writer_t* w) {
        for (int i = 0; i < JKII_WRITER_MAX_DEPTH; ++i) jkii_write_array_start(w);
        for (int i = 0; i < JKII_WRITER_MAX_DEPTH; ++i) jkii_write_array_end(w);
    }) == expected );
    CHECK( into_buffer([](jkii_writer_t* w) {
        for (int i = 0; i <= JKII_WRITER_MAX_DEPTH; ++i) jkii_write_array_start(w);
    }).err == JKII_WRITE_ERR_TOO_DEEP );
}

TEST_CASE( "The JSON writer streams through a callback, a buffer at a time" ) {
    for (size_t buff_size : { 1, 2, 7, 64, 1024 }) {
        INFO( "buffer of " << buff_size );
        int calls = 0;
        Written written = through_callback(state, buff_size, &calls);
        CHECK( written.err == JKII_WRITE_OK );
        CHECK( written.json == STATE );
        CHECK( written.len == STATE.size() );
        CHECK( calls == (int)((STATE.size() + buff_size - 1) / buff_size) );
    }

    SECTION("a callback that takes less fails the writer") {
        char buff[8];
        jkii_writer_t w;
        int calls = 0;
        jkii_writer_init_cb(&w, buff, sizeof(buff),
                [](const char*, size_t size, void* userdata) {
                    ++*(int*)userdata;
                    return size - 1;
                }, &calls);
        state(&w);
        CHECK( jkii_writer_finish(&w, nullptr) == JKII_WRITE_ERR_CALLBACK );
        CHECK( calls == 1 );
    }
}

TEST_CASE( "The JSON writer writes JSON Lines, a record at a time" ) {
    const Writes records = [](jkii_writer_t* w) {
        for (int i = 0; i < 3; ++i) {
            jkii_write_object_start(w);
            jkii_write_key(w, "seq");
            jkii_write_int(w, i);
            jkii_write_key(w, "note");
            jkii_write_string(w, "a\nb");
            jkii_write_object_end(w);
            jkii_write_record_end(w);
        }
    };
    const std::string lines =
        "{\"seq\":0,\"note\":\"a\\nb\"}\n"
        "{\"seq\":1,\"note\":\"a\\nb\"}\n"
        "{\"seq\":2,\"note\":\"a\\nb\"}\n";
    CHECK( json_of(records) == lines );

    SECTION("through a callback, a buffer at a time") {
        Written written = through_callback(records, 5);
        CHECK( written.err == JKII_WRITE_OK );
        CHECK( written.json == lines );
        CHECK( written.len == lines.size() );
    }

    SECTION("a dry run counts the line ends") {
        jkii_writer_t w;
        jkii_writer_init_buff(&w, nullptr, 0);
        records(&w);
        size_t needed = 0;
        CHECK( jkii_writer_finish(&w, &needed) == JKII_WRITE_OK );
        CHECK( needed == lines.size() );
    }

    SECTION("a value of any kind is a record") {
        CHECK( json_of([](jkii_writer_t* w) {
            jkii_write_int(w, 1);
            jkii_write_record_end(w);
            jkii_write_string(w, "two");
            jkii_write_record_end(w);
            jkii_write_array_start(w);
            jkii_write_array_end(w);
        }) == "1\n\"two\"\n[]" );
    }

    SECTION("only after a whole value") {
        CHECK( into_buffer([](jkii_writer_t* w) { jkii_write_record_end(w); }).err == JKII_WRITE_ERR_STRUCTURE );
        CHECK( into_buffer([](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_record_end(w);
        }).err == JKII_WRITE_ERR_STRUCTURE );
        CHECK( into_buffer([](jkii_writer_t* w) {
            jkii_write_null(w);
            jkii_write_record_end(w);
            jkii_write_record_end(w);
        }).err == JKII_WRITE_ERR_STRUCTURE );
        // A record left open at the end.
        CHECK( into_buffer([](jkii_writer_t* w) {
            jkii_write_null(w);
            jkii_write_record_end(w);
            jkii_write_array_start(w);
        }).err == JKII_WRITE_ERR_STRUCTURE );
    }
}

// A file of JSON Lines grows a record at a time, each written by a writer of
// its own and appended.
TEST_CASE( "Records written one at a time append to JSON Lines" ) {
    std::string file;
    for (int i = 0; i < 3; ++i) {
        Written written = into_buffer([i](jkii_writer_t* w) {
            jkii_write_object_start(w);
            jkii_write_key(w, "seq");
            jkii_write_int(w, i);
            jkii_write_object_end(w);
            jkii_write_record_end(w);
        }, 32);
        REQUIRE( written.err == JKII_WRITE_OK );
        REQUIRE( written.len == written.json.size() );
        file += written.json;
    }
    CHECK( file == "{\"seq\":0}\n{\"seq\":1}\n{\"seq\":2}\n" );

    // A record that doesn't fit is refused whole, with the length it needs, so
    // that nothing of it is appended.
    Written too_large = into_buffer([](jkii_writer_t* w) {
        jkii_write_string(w, "a record longer than the buffer");
        jkii_write_record_end(w);
    }, 16);
    CHECK( too_large.err == JKII_WRITE_ERR_TOO_LARGE );
    CHECK( too_large.len == strlen("\"a record longer than the buffer\"\n") );
}
