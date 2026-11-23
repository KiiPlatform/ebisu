#include <catch.hpp>
#include <jkii.h>
#include <jkii_utils.h>
#include <string.h>
#include <string>

// Escapes str into a buffer with room to spare and returns the result,
// checking that the returned length is what was written.
static std::string escape(const char* str)
{
    char buff[64];
    int len = jkii_escape_str(str, buff, sizeof(buff));
    REQUIRE(len >= 0);
    REQUIRE(strlen(buff) == (size_t)len);
    return std::string(buff, len);
}

TEST_CASE("KiiJson, EscapeStrPlain")
{
    REQUIRE(escape("") == "");
    REQUIRE(escape("vendor-thing_id 01") == "vendor-thing_id 01");
    // Bytes from 0x80 up are UTF-8 and pass through. Read as a signed char
    // they are negative, which a "below 0x20" test must not mistake for a
    // control character.
    REQUIRE(escape("caf\xc3\xa9") == "caf\xc3\xa9");
    // Neither space nor DEL needs escaping in JSON.
    REQUIRE(escape(" \x7f") == " \x7f");
}

TEST_CASE("KiiJson, EscapeStrQuoteBackslashSlash")
{
    REQUIRE(escape("\"") == "\\\"");
    REQUIRE(escape("\\") == "\\\\");
    REQUIRE(escape("/") == "\\/");
    REQUIRE(escape("a\"b\\c/d") == "a\\\"b\\\\c\\/d");
}

// These used to come out as a backslash followed by the raw control byte,
// which is not a JSON escape at all.
TEST_CASE("KiiJson, EscapeStrShortControlEscapes")
{
    REQUIRE(escape("\b") == "\\b");
    REQUIRE(escape("\f") == "\\f");
    REQUIRE(escape("\n") == "\\n");
    REQUIRE(escape("\r") == "\\r");
    REQUIRE(escape("\t") == "\\t");
    REQUIRE(escape("a\r\nb\tc") == "a\\r\\nb\\tc");
}

// JSON has no short form for the other control characters, and does not
// allow them unescaped. These used to be copied through as they were.
TEST_CASE("KiiJson, EscapeStrOtherControlCharacters")
{
    REQUIRE(escape("\x01") == "\\u0001");
    REQUIRE(escape("\x1f") == "\\u001f");
    REQUIRE(escape("\x0b") == "\\u000b");
    REQUIRE(escape("a\x1b[0m") == "a\\u001b[0m");
}

TEST_CASE("KiiJson, EscapeStrBufferBoundary")
{
    char buff[16];
    const std::string untouched(sizeof(buff), 'X');

    // Unescaped: three characters and the terminator.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(3 == jkii_escape_str("abc", buff, 4));
    REQUIRE(std::string(buff) == "abc");
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("abc", buff, 3));
    REQUIRE(std::string(buff + 3, sizeof(buff) - 3) == untouched.substr(3));

    // A two-character escape: "a\nb" becomes a \ n b.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(4 == jkii_escape_str("a\nb", buff, 5));
    REQUIRE(std::string(buff) == "a\\nb");
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("a\nb", buff, 4));
    REQUIRE(std::string(buff + 4, sizeof(buff) - 4) == untouched.substr(4));

    // A six-character escape needs seven bytes, six times its input.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(6 == jkii_escape_str("\x01", buff, 7));
    REQUIRE(std::string(buff) == "\\u0001");
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("\x01", buff, 6));
    REQUIRE(std::string(buff + 6, sizeof(buff) - 6) == untouched.substr(6));
}

// The kii callers do not check the result before printing the buffer with
// %s, so a failure must not leave it unterminated.
TEST_CASE("KiiJson, EscapeStrFailureLeavesEmptyString")
{
    char buff[16];

    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("abc", buff, 3));
    REQUIRE('\0' == buff[0]);

    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("ab\ncd", buff, 6));
    REQUIRE('\0' == buff[0]);

    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("ab\x01", buff, 8));
    REQUIRE('\0' == buff[0]);

    // With no room at all, nothing is written.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_escape_str("", buff, 0));
    REQUIRE('X' == buff[0]);
}
