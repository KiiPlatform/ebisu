#include <catch.hpp>
#include <jkii.h>
#include <jkii_utils.h>
#include <jkii_writer.h>
#include <string.h>
#include <string>

// Unescapes the characters of s into a buffer with room to spare and returns
// the result, checking that the returned length is what was written.
static std::string unescape(const std::string& s)
{
    char buff[64];
    int len = jkii_unescape_str(s.data(), s.size(), buff, sizeof(buff));
    REQUIRE(len >= 0);
    REQUIRE(strlen(buff) == (size_t)len);
    return std::string(buff, len);
}

// Whether s is refused, leaving an empty string.
static bool refused(const std::string& s)
{
    char buff[64];
    memset(buff, 'X', sizeof(buff));
    int len = jkii_unescape_str(s.data(), s.size(), buff, sizeof(buff));
    return len == -1 && buff[0] == '\0';
}

TEST_CASE("KiiJson, UnescapeStrPlain")
{
    REQUIRE(unescape("") == "");
    REQUIRE(unescape("vendor-thing_id 01") == "vendor-thing_id 01");
    REQUIRE(unescape("caf\xc3\xa9") == "caf\xc3\xa9");
    REQUIRE(unescape(" \x7f") == " \x7f");
}

TEST_CASE("KiiJson, UnescapeStrShortEscapes")
{
    REQUIRE(unescape("\\\"") == "\"");
    REQUIRE(unescape("\\\\") == "\\");
    REQUIRE(unescape("\\/") == "/");
    REQUIRE(unescape("\\b") == "\b");
    REQUIRE(unescape("\\f") == "\f");
    REQUIRE(unescape("\\n") == "\n");
    REQUIRE(unescape("\\r") == "\r");
    REQUIRE(unescape("\\t") == "\t");
    REQUIRE(unescape("a\\\"b\\\\c\\/d\\r\\ne") == "a\"b\\c/d\r\ne");
}

TEST_CASE("KiiJson, UnescapeStrUnicodeEscapes")
{
    // One, two and three bytes of UTF-8, the hex digits in either case.
    REQUIRE(unescape("\\u0041") == "A");
    REQUIRE(unescape("\\u001f") == "\x1f");
    REQUIRE(unescape("\\u007f") == "\x7f");
    REQUIRE(unescape("\\u0080") == "\xc2\x80");
    REQUIRE(unescape("caf\\u00e9") == "caf\xc3\xa9");
    REQUIRE(unescape("caf\\u00E9") == "caf\xc3\xa9");
    REQUIRE(unescape("\\u07ff") == "\xdf\xbf");
    REQUIRE(unescape("\\u0800") == "\xe0\xa0\x80");
    REQUIRE(unescape("\\u20aC") == "\xe2\x82\xac");
    REQUIRE(unescape("\\uffff") == "\xef\xbf\xbf");
    // Beyond U+FFFF, a pair: four bytes.
    REQUIRE(unescape("\\ud800\\udc00") == "\xf0\x90\x80\x80");
    REQUIRE(unescape("\\ud83d\\ude00") == "\xf0\x9f\x98\x80");
    REQUIRE(unescape("\\uDBFF\\uDFFF") == "\xf4\x8f\xbf\xbf");
    REQUIRE(unescape("a\\ud83d\\ude00b") == "a\xf0\x9f\x98\x80" "b");
}

TEST_CASE("KiiJson, UnescapeStrRefused")
{
    // Escapes JSON doesn't have, or cut short.
    REQUIRE(refused("\\x"));
    REQUIRE(refused("\\a"));
    REQUIRE(refused("\\'"));
    REQUIRE(refused("\\U0041"));
    REQUIRE(refused("\\"));
    REQUIRE(refused("abc\\"));
    REQUIRE(refused("\\u"));
    REQUIRE(refused("\\u004"));
    REQUIRE(refused("\\u004g"));
    REQUIRE(refused("\\u+041"));
    REQUIRE(refused("\\u 041"));
    // A NUL would end the string.
    REQUIRE(refused("\\u0000"));
    REQUIRE(refused("a\\u0000b"));
    // Surrogates that aren't a high one followed by a low one.
    REQUIRE(refused("\\ud83d"));
    REQUIRE(refused("\\ud83dx"));
    REQUIRE(refused("\\ud83d\\n"));
    REQUIRE(refused("\\ud83d\\u0041"));
    REQUIRE(refused("\\ud83d\\ud83d"));
    REQUIRE(refused("\\ud83d\\ude0"));
    REQUIRE(refused("\\ude00"));
    REQUIRE(refused("\\ude00\\ud83d"));
    // What a JSON string can't hold unescaped.
    REQUIRE(refused("\""));
    REQUIRE(refused("a\"b"));
    REQUIRE(refused("\x01"));
    REQUIRE(refused("a\nb"));
    REQUIRE(refused(std::string("a\0b", 3)));
}

// The characters need no NUL after them, and none past str_len is read.
TEST_CASE("KiiJson, UnescapeStrNotTerminated")
{
    char buff[16];
    const char str[] = { 'a', '\\', 'n', 'b', '\\', 'u' };
    REQUIRE(3 == jkii_unescape_str(str, 4, buff, sizeof(buff)));
    REQUIRE(std::string(buff) == "a\nb");
    // The last escape, cut off by str_len, is refused.
    REQUIRE(-1 == jkii_unescape_str(str, sizeof(str), buff, sizeof(buff)));
    REQUIRE(-1 == jkii_unescape_str(str, 2, buff, sizeof(buff)));
    REQUIRE(0 == jkii_unescape_str(NULL, 0, buff, sizeof(buff)));
    REQUIRE(std::string(buff) == "");
}

TEST_CASE("KiiJson, UnescapeStrBufferBoundary")
{
    char buff[16];
    const std::string untouched(sizeof(buff), 'X');

    // Three characters and the terminator.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(3 == jkii_unescape_str("abc", 3, buff, 4));
    REQUIRE(std::string(buff) == "abc");
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_unescape_str("abc", 3, buff, 3));
    REQUIRE('\0' == buff[0]);
    REQUIRE(std::string(buff + 3, sizeof(buff) - 3) == untouched.substr(3));

    // A pair: twelve characters, four bytes and the terminator.
    memset(buff, 'X', sizeof(buff));
    REQUIRE(4 == jkii_unescape_str("\\ud83d\\ude00", 12, buff, 5));
    REQUIRE(std::string(buff) == "\xf0\x9f\x98\x80");
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_unescape_str("\\ud83d\\ude00", 12, buff, 4));
    REQUIRE('\0' == buff[0]);
    REQUIRE(std::string(buff + 4, sizeof(buff) - 4) == untouched.substr(4));

    // With no room at all, nothing is written, even for "".
    memset(buff, 'X', sizeof(buff));
    REQUIRE(-1 == jkii_unescape_str("", 0, buff, 0));
    REQUIRE(-1 == jkii_unescape_str("abc", 3, buff, 0));
    REQUIRE('X' == buff[0]);
}

// What jkii_escape_str() escapes, jkii_unescape_str() gives back, in a buffer
// of the escaped length and the terminator.
TEST_CASE("KiiJson, UnescapeStrUndoesEscapeStr")
{
    const char* strs[] = {
        "",
        "plain",
        "a/b",
        "\"quoted\" \\ back",
        "\b\f\n\r\t",
        "\x01\x1b[0m\x1f",
        "caf\xc3\xa9 \xf0\x9f\x98\x80",
    };
    for (const char* str : strs) {
        char escaped[128];
        char buff[128];
        int escaped_len = jkii_escape_str(str, escaped, sizeof(escaped));
        REQUIRE(escaped_len >= 0);
        int len = jkii_unescape_str(escaped, (size_t)escaped_len, buff,
                (size_t)escaped_len + 1);
        REQUIRE(len == (int)strlen(str));
        REQUIRE(std::string(buff) == str);
    }
}

// A key the writer wrote is found again by its name once unescaped: the
// writer escapes '/', and jkii_next_member() gives the key as written.
TEST_CASE("KiiJson, UnescapeStrKeyFromNextMember")
{
    char json[64];
    size_t json_len = 0;
    jkii_writer_t w;
    jkii_writer_init_buff(&w, json, sizeof(json));
    jkii_write_object_start(&w);
    jkii_write_key(&w, "sensors/1");
    jkii_write_string(&w, "line 1\nline 2");
    jkii_write_object_end(&w);
    REQUIRE(JKII_WRITE_OK == jkii_writer_finish(&w, &json_len));
    REQUIRE(std::string(json) == "{\"sensors\\/1\":\"line 1\\nline 2\"}");

    jkii_member_iter_t iter;
    jkii_member_t member;
    memset(&iter, 0x00, sizeof(iter));
    REQUIRE(JKII_ERR_OK == jkii_next_member(json, json_len, &iter, &member));
    REQUIRE(std::string(member.key, member.key_length) == "sensors\\/1");

    char key[16];
    REQUIRE(9 == jkii_unescape_str(member.key, member.key_length, key, sizeof(key)));
    REQUIRE(std::string(key) == "sensors/1");
    char value[16];
    REQUIRE(13 == jkii_unescape_str(member.value, member.value_length, value,
                sizeof(value)));
    REQUIRE(std::string(value) == "line 1\nline 2");
}
