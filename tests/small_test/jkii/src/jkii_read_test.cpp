// The readers that find a value and read it in one call, jkii_find_integer()
// and the others; a member of the root by a plain name, jkii_find_name(); a
// scalar as text, jkii_member_text(); and a string cut to fit,
// jkii_member_string_trunc().
#include <catch.hpp>
#include <jkii.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

// A document as a response's buffer has it: not terminated.
struct Doc {
    std::vector<char> bytes;
    explicit Doc(const std::string& s) : bytes(s.begin(), s.end()) {}
    // Never NULL, which jkii takes for no document at all.
    const char* data() const { return bytes.empty() ? "" : bytes.data(); }
    size_t size() const { return bytes.size(); }
};

std::string written(const jkii_member_t& m)
{
    return m.value != nullptr ? std::string(m.value, m.value_length) : "(none)";
}

// A member set to nothing, to tell whether a reader set it.
jkii_member_t untouched()
{
    jkii_member_t m;
    memset(&m, 0, sizeof(m));
    return m;
}

const Doc NUMBERS(R"({ "i" : 30, "f" : 30.0, "e" : 3e1, "frac" : 30.5, "neg" : -5,
    "q" : "30", "qf" : "30.0", "qfrac" : "30.5", "n" : null, "s" : "x",
    "big" : 1e400, "o" : { "deep" : [ 1, 2 ] } })");

}

TEST_CASE( "jkii_find_integer() finds and reads an integer in one call" ) {
    int64_t v = 7;
    jkii_member_t found = untouched();
    CHECK( jkii_find_integer(NUMBERS.data(), NUMBERS.size(), "/i", 1, 3600, 0, &v, &found)
            == JKII_FIELD_ERR_OK );
    CHECK( v == 30 );
    CHECK( written(found) == "30" );
    CHECK( found.type == JKII_FIELD_TYPE_INTEGER );
    CHECK( jkii_find_integer(NUMBERS.data(), NUMBERS.size(), "/o/deep/[1]", 0, 9, 0, &v, nullptr)
            == JKII_FIELD_ERR_OK );
    CHECK( v == 2 );
    CHECK( jkii_find_integer(NUMBERS.data(), NUMBERS.size(), "/neg", -5, -5, 0, &v, nullptr)
            == JKII_FIELD_ERR_OK );
    CHECK( v == -5 );
}

TEST_CASE( "jkii_find_integer() says what it found, absent, null or refused" ) {
    struct Case {
        const char* path;
        int64_t min, max;
        unsigned int flags;
        jkii_field_err_t res;
        const char* found;
    };
    const Case cases[] = {
        { "/missing", 0, 100, 0, JKII_FIELD_ERR_NOT_FOUND, "(none)" },
        { "", 0, 100, 0, JKII_FIELD_ERR_NOT_FOUND, "(none)" },
        { "/n", 0, 100, 0, JKII_FIELD_ERR_NULL, "null" },
        { "/n", 0, 100, JKII_READ_NULL_AS_ABSENT, JKII_FIELD_ERR_NOT_FOUND, "null" },
        { "/s", 0, 100, 0, JKII_FIELD_ERR_TYPE_MISMATCH, "x" },
        { "/o", 0, 100, 0, JKII_FIELD_ERR_TYPE_MISMATCH, R"({ "deep" : [ 1, 2 ] })" },
        { "/i", 31, 100, 0, JKII_FIELD_ERR_NUM_UNDERFLOW, "30" },
        { "/i", 0, 29, 0, JKII_FIELD_ERR_NUM_OVERFLOW, "30" },
        { "/frac", 0, 100, 0, JKII_FIELD_ERR_TYPE_MISMATCH, "30.5" },
        // A whole number written otherwise, unless asked for as written.
        { "/f", 0, 100, JKII_READ_INTEGER_AS_WRITTEN, JKII_FIELD_ERR_TYPE_MISMATCH, "30.0" },
        { "/e", 0, 100, JKII_READ_INTEGER_AS_WRITTEN, JKII_FIELD_ERR_TYPE_MISMATCH, "3e1" },
        // A number in a string, unless asked for, and then as the number.
        { "/q", 0, 100, 0, JKII_FIELD_ERR_TYPE_MISMATCH, "30" },
        { "/qfrac", 0, 100, JKII_READ_UNQUOTE, JKII_FIELD_ERR_TYPE_MISMATCH, "30.5" },
        { "/qf", 0, 100, JKII_READ_UNQUOTE | JKII_READ_INTEGER_AS_WRITTEN,
            JKII_FIELD_ERR_TYPE_MISMATCH, "30.0" },
        { "/q", 31, 100, JKII_READ_UNQUOTE, JKII_FIELD_ERR_NUM_UNDERFLOW, "30" },
    };
    for (const Case& c : cases) {
        INFO( c.path << " with flags " << c.flags );
        int64_t v = 7;
        jkii_member_t found = untouched();
        CHECK( jkii_find_integer(NUMBERS.data(), NUMBERS.size(), c.path, c.min, c.max, c.flags,
                    &v, &found) == c.res );
        CHECK( v == 7 );
        CHECK( written(found) == c.found );
    }

    // What the flags take.
    struct Taken {
        const char* path;
        unsigned int flags;
    };
    for (const Taken& t : { Taken{ "/f", 0 }, Taken{ "/e", 0 }, Taken{ "/q", JKII_READ_UNQUOTE },
                Taken{ "/qf", JKII_READ_UNQUOTE },
                Taken{ "/q", JKII_READ_UNQUOTE | JKII_READ_INTEGER_AS_WRITTEN },
                Taken{ "/i", JKII_READ_UNQUOTE | JKII_READ_INTEGER_AS_WRITTEN |
                    JKII_READ_NULL_AS_ABSENT } }) {
        INFO( t.path << " with flags " << t.flags );
        int64_t v = 7;
        jkii_member_t found = untouched();
        CHECK( jkii_find_integer(NUMBERS.data(), NUMBERS.size(), t.path, 0, 100, t.flags, &v,
                    &found) == JKII_FIELD_ERR_OK );
        CHECK( v == 30 );
        // The value as written, a string's characters for one in a string.
        CHECK( found.type == (t.path[1] == 'q' ? JKII_FIELD_TYPE_STRING : found.type) );
    }
}

TEST_CASE( "The finding readers refuse what isn't a document of members" ) {
    for (const char* json : { "{", "", " ", "30", R"("s")", "null", "{\"a\":1} x" }) {
        INFO( json );
        const Doc d(json);
        int64_t i = 7;
        double f = 7;
        bool b = true;
        char s[8] = "xxxxxxx";
        jkii_member_t found = untouched();
        CHECK( jkii_find_integer(d.data(), d.size(), "/a", 0, 9, 0, &i, &found)
                == JKII_FIELD_ERR_INVALID_INPUT );
        CHECK( jkii_find_double(d.data(), d.size(), "/a", 0, &f, &found)
                == JKII_FIELD_ERR_INVALID_INPUT );
        CHECK( jkii_find_boolean(d.data(), d.size(), "/a", 0, &b, &found)
                == JKII_FIELD_ERR_INVALID_INPUT );
        CHECK( jkii_find_string(d.data(), d.size(), "/a", 0, s, sizeof(s), &found)
                == JKII_FIELD_ERR_INVALID_INPUT );
        CHECK( i == 7 );
        CHECK( f == 7 );
        CHECK( b == true );
        CHECK( s[0] == '\0' );
        CHECK( (const void*)found.value == nullptr );
    }
}

TEST_CASE( "jkii_find_double() and jkii_find_boolean() find and read in one call" ) {
    double f = 7;
    jkii_member_t found = untouched();
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/frac", 0, &f, &found)
            == JKII_FIELD_ERR_OK );
    CHECK( f == 30.5 );
    CHECK( written(found) == "30.5" );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/qfrac", 0, &f, nullptr)
            == JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/qfrac", JKII_READ_UNQUOTE, &f,
                nullptr) == JKII_FIELD_ERR_OK );
    CHECK( f == 30.5 );
    f = 7;
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/big", 0, &f, &found)
            == JKII_FIELD_ERR_NUM_OVERFLOW );
    CHECK( written(found) == "1e400" );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/n", 0, &f, nullptr)
            == JKII_FIELD_ERR_NULL );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/n", JKII_READ_NULL_AS_ABSENT, &f,
                nullptr) == JKII_FIELD_ERR_NOT_FOUND );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/s", 0, &f, nullptr)
            == JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( jkii_find_double(NUMBERS.data(), NUMBERS.size(), "/none", 0, &f, nullptr)
            == JKII_FIELD_ERR_NOT_FOUND );
    CHECK( f == 7 );

    const Doc d(R"({"on": true, "q": "false", "n": null, "one": 1})");
    bool b = false;
    CHECK( jkii_find_boolean(d.data(), d.size(), "/on", 0, &b, &found) == JKII_FIELD_ERR_OK );
    CHECK( b == true );
    CHECK( jkii_find_boolean(d.data(), d.size(), "/q", 0, &b, &found)
            == JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( written(found) == "false" );
    CHECK( jkii_find_boolean(d.data(), d.size(), "/q", JKII_READ_UNQUOTE, &b, nullptr)
            == JKII_FIELD_ERR_OK );
    CHECK( b == false );
    b = true;
    CHECK( jkii_find_boolean(d.data(), d.size(), "/n", 0, &b, nullptr) == JKII_FIELD_ERR_NULL );
    CHECK( jkii_find_boolean(d.data(), d.size(), "/n", JKII_READ_NULL_AS_ABSENT, &b, nullptr)
            == JKII_FIELD_ERR_NOT_FOUND );
    CHECK( jkii_find_boolean(d.data(), d.size(), "/one", JKII_READ_UNQUOTE, &b, nullptr)
            == JKII_FIELD_ERR_TYPE_MISMATCH );
    CHECK( jkii_find_boolean(d.data(), d.size(), "/none", 0, &b, nullptr)
            == JKII_FIELD_ERR_NOT_FOUND );
    CHECK( b == true );
}

TEST_CASE( "jkii_find_string() finds and copies a string in one call" ) {
    const Doc d(R"({"s": "caf\u00e9", "n": null, "i": 3})");
    char s[8] = "xxxxxxx";
    jkii_member_t found = untouched();
    CHECK( jkii_find_string(d.data(), d.size(), "/s", 0, s, sizeof(s), &found)
            == JKII_FIELD_ERR_OK );
    CHECK( std::string(s) == "caf\xc3\xa9" );
    CHECK( written(found) == R"(caf\u00e9)" );
    // A byte short.
    char small[5] = "xxxx";
    CHECK( jkii_find_string(d.data(), d.size(), "/s", 0, small, sizeof(small), nullptr)
            == JKII_FIELD_ERR_COPY );
    CHECK( small[0] == '\0' );
    struct Case {
        const char* path;
        unsigned int flags;
        jkii_field_err_t res;
    };
    for (const Case& c : { Case{ "/n", 0, JKII_FIELD_ERR_NULL },
                Case{ "/n", JKII_READ_NULL_AS_ABSENT, JKII_FIELD_ERR_NOT_FOUND },
                Case{ "/i", JKII_READ_UNQUOTE, JKII_FIELD_ERR_TYPE_MISMATCH },
                Case{ "/none", 0, JKII_FIELD_ERR_NOT_FOUND } }) {
        INFO( c.path );
        char b[8] = "xxxxxxx";
        CHECK( jkii_find_string(d.data(), d.size(), c.path, c.flags, b, sizeof(b), nullptr)
                == c.res );
        CHECK( b[0] == '\0' );
    }
}

TEST_CASE( "jkii_find_name() finds a member of the root by its plain name" ) {
    const Doc d(R"({ "a/b" : 1, "[0]" : 2, "a.b" : 3, "" : 4, "a\u0062" : 5, "ab" : 6,
        "k\u0000z" : 7, "dup" : 8, "dup" : 9, "o" : { "x" : 10 }, "caf\u00e9" : 11 })");
    struct Case {
        std::string name;
        const char* value;
        const char* key;
    };
    const Case cases[] = {
        { "a/b", "1", "a/b" },
        { "[0]", "2", "[0]" },
        { "a.b", "3", "a.b" },
        { "", "4", "" },
        // Compared unescaped: the first of the two that are ab.
        { "ab", "5", R"(a\u0062)" },
        { std::string("k\0z", 3), "7", R"(k\u0000z)" },
        { "dup", "8", "dup" },
        { "caf\xc3\xa9", "11", R"(caf\u00e9)" },
    };
    for (const Case& c : cases) {
        INFO( c.name );
        jkii_member_t m = untouched();
        REQUIRE( jkii_find_name(d.data(), d.size(), c.name.data(), c.name.size(), &m)
                == JKII_ERR_OK );
        CHECK( written(m) == c.value );
        REQUIRE( (const void*)m.key != nullptr );
        CHECK( std::string(m.key, m.key_length) == c.key );
        CHECK( m.type == JKII_FIELD_TYPE_INTEGER );
    }
    jkii_member_t m = untouched();
    CHECK( jkii_find_name(d.data(), d.size(), nullptr, 0, &m) == JKII_ERR_OK );
    CHECK( written(m) == "4" );
    // Not a prefix, nor one further in, nor a name as written.
    for (const char* name : { "a", "abc", "x", "nope", R"(a\u0062)", "k" }) {
        INFO( name );
        jkii_member_t n = untouched();
        CHECK( jkii_find_name(d.data(), d.size(), name, strlen(name), &n) == JKII_ERR_NOT_FOUND );
        CHECK( (const void*)n.value == nullptr );
    }
    for (const char* json : { "[1]", "1", R"("a")" }) {
        INFO( json );
        const Doc other(json);
        CHECK( jkii_find_name(other.data(), other.size(), "a", 1, &m) == JKII_ERR_ROOT_TYPE );
    }
    const Doc broken("{\"a\":");
    CHECK( jkii_find_name(broken.data(), broken.size(), "a", 1, &m) == JKII_ERR_INVALID_INPUT );
}

TEST_CASE( "jkii_member_text() gives a scalar as text" ) {
    const Doc d(R"({"s": "caf\u00e9\n", "i": 12, "f": 2.5e3, "z": -0, "t": true,
        "fa": false, "n": null, "o": {}, "a": [], "nul": "a\u0000"})");
    struct Case {
        const char* path;
        jkii_field_err_t res;
        const char* text;
    };
    const Case cases[] = {
        { "/s", JKII_FIELD_ERR_OK, "caf\xc3\xa9\n" },
        { "/i", JKII_FIELD_ERR_OK, "12" },
        { "/f", JKII_FIELD_ERR_OK, "2.5e3" },
        { "/z", JKII_FIELD_ERR_OK, "-0" },
        { "/t", JKII_FIELD_ERR_OK, "true" },
        { "/fa", JKII_FIELD_ERR_OK, "false" },
        { "/n", JKII_FIELD_ERR_NULL, "" },
        { "/o", JKII_FIELD_ERR_TYPE_MISMATCH, "" },
        { "/a", JKII_FIELD_ERR_TYPE_MISMATCH, "" },
        { "/nul", JKII_FIELD_ERR_COPY, "" },
    };
    for (const Case& c : cases) {
        INFO( c.path );
        jkii_member_t m;
        REQUIRE( jkii_find_member(d.data(), d.size(), c.path, &m) == JKII_ERR_OK );
        char text[16] = "xxxxxxxxxxxxxxx";
        CHECK( jkii_member_text(&m, text, sizeof(text)) == c.res );
        CHECK( std::string(text) == c.text );
    }

    // Exactly the room it takes, and a byte short.
    jkii_member_t m;
    REQUIRE( jkii_find_member(d.data(), d.size(), "/f", &m) == JKII_ERR_OK );
    char exact[6];
    CHECK( jkii_member_text(&m, exact, sizeof(exact)) == JKII_FIELD_ERR_OK );
    char short_buff[5] = "xxxx";
    CHECK( jkii_member_text(&m, short_buff, sizeof(short_buff)) == JKII_FIELD_ERR_COPY );
    CHECK( short_buff[0] == '\0' );
    CHECK( jkii_member_text(&m, nullptr, 0) == JKII_FIELD_ERR_COPY );

    // A number longer than jkii reads, as written all the same.
    const std::string longest = "1." + std::string(400, '5');
    const Doc l("[" + longest + "]");
    jkii_member_iter_t it = { 0, 0 };
    REQUIRE( jkii_next_member(l.data(), l.size(), &it, &m) == JKII_ERR_OK );
    CHECK( m.type == JKII_FIELD_TYPE_ANY );
    std::vector<char> big(longest.size() + 1);
    CHECK( jkii_member_text(&m, big.data(), big.size()) == JKII_FIELD_ERR_OK );
    CHECK( std::string(big.data()) == longest );
}

namespace {

struct Cut {
    jkii_field_err_t res;
    std::string text;
    bool truncated;
};

Cut cut_once(const jkii_member_t& m, size_t size, bool truncated_before)
{
    std::vector<char> buff(size + 1, 'x');
    Cut c;
    c.truncated = truncated_before;
    c.res = jkii_member_string_trunc(&m, size > 0 ? buff.data() : nullptr, size, &c.truncated);
    c.text = size > 0 ? std::string(buff.data()) : "";
    // Nothing past the buffer.
    CHECK( buff[size] == 'x' );
    return c;
}

Cut cut(const char* json, size_t size)
{
    jkii_member_t m;
    REQUIRE( jkii_root_value(json, strlen(json), &m) == JKII_ERR_OK );
    // With the flag set each way before the call, to see that it's written
    // either way.
    Cut c = cut_once(m, size, true);
    Cut other = cut_once(m, size, false);
    CHECK( other.res == c.res );
    CHECK( other.text == c.text );
    CHECK( other.truncated == c.truncated );
    return c;
}

bool is_utf8(const std::string& s)
{
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        size_t n = c < 0x80 ? 1 : c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 0;
        if (n == 0 || i + n > s.size()) {
            return false;
        }
        for (size_t k = 1; k < n; ++k) {
            if (((unsigned char)s[i + k] & 0xc0) != 0x80) {
                return false;
            }
        }
        i += n;
    }
    return true;
}

}

TEST_CASE( "jkii_member_string_trunc() copies what fits, cut between characters" ) {
    Cut c = cut(R"("abc")", 4);
    CHECK( c.res == JKII_FIELD_ERR_OK );
    CHECK( c.text == "abc" );
    CHECK( c.truncated == false );
    c = cut(R"("abc")", 3);
    CHECK( c.res == JKII_FIELD_ERR_OK );
    CHECK( c.text == "ab" );
    CHECK( c.truncated == true );
    c = cut(R"("abc")", 1);
    CHECK( c.text == "" );
    CHECK( c.truncated == true );
    c = cut(R"("")", 1);
    CHECK( c.text == "" );
    CHECK( c.truncated == false );
    c = cut(R"("abc")", 0);
    CHECK( c.res == JKII_FIELD_ERR_COPY );

    // Not inside a character's bytes: an escaped one, one written as UTF-8,
    // and a surrogate pair's four bytes.
    CHECK( cut(R"("caf\u00e9")", 5).text == "caf" );
    CHECK( cut(R"("caf\u00e9")", 6).text == "caf\xc3\xa9" );
    CHECK( cut("\"x\xe2\x82\xac\"", 4).text == "x" );
    CHECK( cut("\"x\xe2\x82\xac\"", 5).text == "x\xe2\x82\xac" );
    CHECK( cut(R"("a\ud83d\ude00")", 5).text == "a" );
    CHECK( cut(R"("a\ud83d\ude00")", 6).text == "a\xf0\x9f\x98\x80" );

    // At a NUL, which a C string ends at, however large the buffer.
    c = cut(R"("ab\u0000cd")", 64);
    CHECK( c.res == JKII_FIELD_ERR_OK );
    CHECK( c.text == "ab" );
    CHECK( c.truncated == true );

    // NULL for the flag.
    jkii_member_t m;
    REQUIRE( jkii_root_value("\"abc\"", 5, &m) == JKII_ERR_OK );
    char two[2];
    CHECK( jkii_member_string_trunc(&m, two, sizeof(two), nullptr) == JKII_FIELD_ERR_OK );
    CHECK( std::string(two) == "a" );

    for (const char* json : { "null", "1", "true", "[]" }) {
        INFO( json );
        jkii_member_t other;
        REQUIRE( jkii_root_value(json, strlen(json), &other) == JKII_ERR_OK );
        char b[4] = "xxx";
        bool t = true;
        CHECK( jkii_member_string_trunc(&other, b, sizeof(b), &t) ==
                (other.type == JKII_FIELD_TYPE_NULL ? JKII_FIELD_ERR_NULL : JKII_FIELD_ERR_TYPE_MISMATCH) );
        CHECK( b[0] == '\0' );
        CHECK( t == false );
    }
}

TEST_CASE( "jkii_member_string_trunc() at every size gives a prefix that is UTF-8" ) {
    const char* json = R"("h\u00e9llo, w\u00f6rld € \ud83d\ude00 \"q\" \\ \n end")";
    jkii_member_t m;
    REQUIRE( jkii_root_value(json, strlen(json), &m) == JKII_ERR_OK );
    std::vector<char> whole(m.value_length + 1);
    REQUIRE( jkii_member_string(&m, whole.data(), whole.size()) == JKII_FIELD_ERR_OK );
    const std::string full(whole.data());
    std::string last;
    for (size_t size = 1; size <= full.size() + 1; ++size) {
        INFO( size );
        Cut c = cut(json, size);
        CHECK( c.res == JKII_FIELD_ERR_OK );
        CHECK( full.compare(0, c.text.size(), c.text) == 0 );
        CHECK( is_utf8(c.text) );
        CHECK( c.text.size() < size );
        CHECK( c.text.size() >= last.size() );
        CHECK( c.truncated == (c.text.size() < full.size()) );
        // As much as fits: the next character wouldn't have.
        if (c.truncated) {
            unsigned char next = (unsigned char)full[c.text.size()];
            size_t n = next < 0x80 ? 1 : next >= 0xf0 ? 4 : next >= 0xe0 ? 3 : 2;
            CHECK( c.text.size() + n + 1 > size );
        }
        last = c.text;
    }
    CHECK( last == full );
}
