#include <catch.hpp>
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
    jkii_write_bool(w, 1);
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
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    char mode[16];
    jkii_field_t fields[3];
    memset(fields, 0, sizeof(fields));
    fields[0].path = "/modes/[1]";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = mode;
    fields[0].field_copy_buff_size = sizeof(mode);
    fields[1].name = "presetTemperature";
    fields[1].type = JKII_FIELD_TYPE_INTEGER;
    CHECK( jkii_parse(json.c_str(), json.size(), fields, &resource) == JKII_ERR_OK );
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
