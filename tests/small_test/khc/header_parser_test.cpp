#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "catch.hpp"
#include <khc.h>
#include "khc_impl.h"

TEST_CASE( "_is_chunked_encoding tests" ) {
    SECTION( "default pattern header" ) {
        const char str[] = "transfer-encoding:chunked\r\n";
        int res = _is_chunked_encoding(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "optional white spaces exist" ) {
        const char str[] = "transfer-encoding: \t chunked\r\n";
        int res = _is_chunked_encoding(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "ignore case" ) {
        const char str[] = "TrAnSfeR-enCodING: chunked\r\n";
        int res = _is_chunked_encoding(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "chunked after other codings" ) {
        const char str[] = "Transfer-Encoding: gzip, compress, chunked\r\n";
        int res = _is_chunked_encoding(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "other header" ) {
        const char str[] = "Content-Length: 0\r\n";
        int res = _is_chunked_encoding(str, strlen(str) - 2);
        REQUIRE( res == 0);
    }
    SECTION( "empty string" ) {
        int res = _is_chunked_encoding("", 0);
        REQUIRE( res == 0);
    }
    SECTION( "NULL" ) {
        int res = _is_chunked_encoding(NULL, 0);
        REQUIRE( res == 0);
    }
}

TEST_CASE( "_is_chunked_encoding matches Transfer-Encoding in any case" ) {
    const std::string names[] = {
        "Transfer-Encoding", "transfer-encoding", "TRANSFER-ENCODING",
        "tRaNsFeR-eNcOdInG"
    };
    for (const auto& name : names) {
        INFO("name [" << name << "]");
        const std::string str = name + ": chunked\r\n";
        CHECK( _is_chunked_encoding(str.data(), str.size() - 2) == 1 );
    }
}

// Only a header named Transfer-Encoding is one. The name was compared up to the
// colon, so any prefix of it, even an empty one, matched and turned on chunked
// decoding, and a NUL in the name matched the terminator of the key and went on
// comparing past its end.
TEST_CASE( "_is_chunked_encoding matches the whole header name" ) {
    const std::string names[] = {
        "", "T", "Transfer", "Transfer-", "Transfer-Encodin",
        "Transfer-Encodings", "Transfer-Encoding ", " Transfer-Encoding",
        "Content-Encoding", std::string("Transfer-Encoding\0", 18),
        std::string("Transfer-Encoding\0\0", 19)
    };
    for (const auto& name : names) {
        INFO("name [" << name << "] of " << name.size());
        const std::string str = name + ": chunked\r\n";
        CHECK( _is_chunked_encoding(str.data(), str.size() - 2) == 0 );
    }
}

// Transfer-Encoding is a list of transfer codings, and the body is chunked when
// chunked is the last of them (RFC 9112, section 6.3). The name of a coding is
// case-insensitive, whitespace may surround it and empty list elements do not
// count (RFC 9110, section 5.6.1).
TEST_CASE( "_is_chunked_encoding finds chunked as the final coding" ) {
    const std::string values[] = {
        "chunked", "Chunked", "CHUNKED", "cHuNkEd", "\t chunked", "chunked \t",
        "gzip, chunked", "gzip,chunked", "gzip \t,\t chunked",
        "gzip, deflate, chunked", "chunked,", "chunked , ", ", chunked",
        "gzip,, chunked"
    };
    for (const auto& value : values) {
        INFO("value [" << value << "]");
        const std::string str = "Transfer-Encoding: " + value + "\r\n";
        CHECK( _is_chunked_encoding(str.data(), str.size() - 2) == 1 );
    }
}

// Any other Transfer-Encoding, an empty one included, is -1, as khc cannot
// remove the coding the body is in. Any value containing the word chunked used
// to be taken for it, chunkedX and chunked, gzip among them.
TEST_CASE( "_is_chunked_encoding rejects a final coding other than chunked" ) {
    const std::string values[] = {
        "", " \t ", ",", " , ", "gzip", "identity", "chunke", "chunkedX",
        "Xchunked", "chunked-gzip", "chunked gzip", "chunked, gzip",
        "gzip, chunked, compress", "chunked,, gzip",
        std::string("chunked\0", 8)
    };
    for (const auto& value : values) {
        INFO("value [" << value << "] of " << value.size());
        const std::string str = "Transfer-Encoding: " + value + "\r\n";
        CHECK( _is_chunked_encoding(str.data(), str.size() - 2) == -1 );
    }
    const char no_value[] = "Transfer-Encoding:";
    CHECK( _is_chunked_encoding(no_value, strlen(no_value)) == -1 );
}

TEST_CASE( "_extract_content_length tests" ) {
    SECTION( "default pattern header exist" ) {
        size_t len = 0;
        const char str[] = "content-length:123\r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 1);
        REQUIRE( len == 123);
    }
    SECTION( "optional white spaces exist ahead" ) {
        size_t len = 0;
        const char str[] = "content-length:\t \t 45678\r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 1);
        REQUIRE( len == 45678);
    }
    SECTION( "optional white spaces exist behind" ) {
        size_t len = 0;
        const char str[] = "content-length:1122 \t \r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 1);
        REQUIRE( len == 1122);
    }
    SECTION( "optional white spaces exist both" ) {
        size_t len = 0;
        const char str[] = "content-length:  \t5656\t  \r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 1);
        REQUIRE( len == 5656);
    }
    SECTION( "ignore case" ) {
        size_t len = 0;
        const char str[] = "cOnTeNt-LenGtH: 1234\r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 1);
        REQUIRE( len == 1234);
    }
    SECTION( "other header" ) {
        size_t len = 0;
        const char str[] = "Transfer-Encoding: Chunked\r\n";
        int res = _extract_content_length(str, strlen(str) - 2, &len);
        REQUIRE( res == 0);
        REQUIRE( len == 0);
    }
    SECTION( "empty string" ) {
        size_t len = 0;
        int res = _extract_content_length("", 0, &len);
        REQUIRE( res == 0);
        REQUIRE( len == 0);
    }
    SECTION( "NULL" ) {
        size_t len = 0;
        int res = _extract_content_length(NULL, 0, &len);
        REQUIRE( res == 0);
        REQUIRE( len == 0);
    }
}

// Content-Length was read with strtol, so the result depended on the width of
// long: on a 32-bit target 2147483648 saturated to LONG_MAX, and SIZE_MAX came
// back as LONG_MAX on 32 and 64 bits alike. A value of one digit was not found
// at all, because the value had to be at least two characters long.
TEST_CASE( "_extract_content_length reads any value that fits a size_t" ) {
    const struct {
        std::string value;
        size_t expected;
    } cases[] = {
        { "0", 0 },
        { "7", 7 },
        { "10", 10 },
        { "2147483648", (size_t)2147483648U },
        { "4294967295", (size_t)4294967295U },
        { std::to_string((size_t)SIZE_MAX), SIZE_MAX },
        { "0000000000000000000000000000001", 1 },
    };
    for (const auto& c : cases) {
        INFO("value " << c.value);
        const std::string str = "Content-Length: " + c.value + "\r\n";
        size_t len = 77;
        int res = _extract_content_length(str.c_str(), str.size() - 2, &len);
        CHECK( res == 1 );
        CHECK( len == c.expected );
    }
}

// Content-Length is 1*DIGIT. strtol took a sign, so -1 was read as SIZE_MAX,
// read junk or nothing as 0, stopped at trailing junk and saturated on
// overflow. Each of these is now an invalid value, which is -1, and the output
// is left alone.
TEST_CASE( "_extract_content_length rejects an invalid value" ) {
    // SIZE_MAX is 2^n - 1, which never ends in 9, so adding one to the last
    // digit carries nowhere.
    std::string over = std::to_string((size_t)SIZE_MAX);
    over.back() += 1;

    const std::string values[] = {
        "-1", "+5", "-0", "abc", "", " \t ", "12abc", "1 2", "0x10", "5, 5",
        over, "123456789012345678901234567890"
    };
    for (const auto& value : values) {
        INFO("value [" << value << "]");
        const std::string str = "Content-Length: " + value + "\r\n";
        size_t len = 77;
        int res = _extract_content_length(str.c_str(), str.size() - 2, &len);
        CHECK( res == -1 );
        CHECK( len == 77 );
    }
}

// Only a header named Content-Length is one. The name was compared up to the
// colon, so any prefix of it, even an empty one, matched, and a NUL in the name
// matched the terminator of the key and went on comparing past its end.
TEST_CASE( "_extract_content_length matches the whole header name" ) {
    const std::string names[] = {
        "", "C", "Content", "Content-Len", "Content-Lengt", "Content-Lengthy",
        "Content-Length ", std::string("Content-Length\0", 15),
        std::string("Content-Length\0\0", 16)
    };
    for (const auto& name : names) {
        INFO("name [" << name << "] of " << name.size());
        const std::string str = name + ": 50\r\n";
        size_t len = 77;
        int res = _extract_content_length(str.data(), str.size() - 2, &len);
        CHECK( res == 0 );
        CHECK( len == 77 );
    }
}

TEST_CASE( "_read_chunk_size test" ) {
    SECTION( "chunk size line exists" ) {
        const char* buff = "123\r\n";
        size_t out_size = 0;
        int res = _read_chunk_size(buff, strlen(buff), &out_size);
        REQUIRE( res == 1);
        REQUIRE( out_size == 0x123 );
    }
    SECTION( "no hex" ) {
        const char* buff = "1z3\r\n";
        size_t out_size = 0;
        int res = _read_chunk_size(buff, strlen(buff), &out_size);
        REQUIRE( res == -1);
        REQUIRE( out_size == 0 );
    }
    SECTION( "LF no exists" ) {
        const char* buff = "123\r";
        size_t out_size = 0;
        int res = _read_chunk_size(buff, strlen(buff), &out_size);
        REQUIRE( res == 0);
        REQUIRE( out_size == 0 );
    }
    SECTION( "CRLF no exists" ) {
        const char* buff = "123";
        size_t out_size = 0;
        int res = _read_chunk_size(buff, strlen(buff), &out_size);
        REQUIRE( res == 0);
        REQUIRE( out_size == 0 );
    }
    SECTION( "empty string" ) {
        const char* buff = "";
        size_t out_size = 0;
        int res = _read_chunk_size(buff, strlen(buff), &out_size);
        REQUIRE( res == 0);
        REQUIRE( out_size == 0 );
    }
}

TEST_CASE( "_read_chunk_size reads sizes up to SIZE_MAX" ) {
    const struct {
        std::string line;
        size_t expected;
    } cases[] = {
        { "0\r\n", 0 },
        { "a\r\n", 0xa },
        { "3ff\r\n", 0x3ff },
        { "3FF\r\n", 0x3ff },
        { "aBcD\r\n", 0xabcd },
        { "7fffffff\r\n", 0x7fffffff },
        { "80000000\r\n", 0x80000000 },
        { "ffffffff\r\n", 0xffffffff },
        { std::string(sizeof(size_t) * 2, 'f') + "\r\n", SIZE_MAX },
        // Leading zeros don't make a size too large.
        { std::string(sizeof(size_t) * 4, '0') + "a\r\n", 0xa },
        // Whitespace and chunk extensions are ignored.
        { "5 \r\n", 5 },
        { "5;name\r\n", 5 },
        { "5;name=value\r\n", 5 },
        { "5 \t;name=\"quoted value\" ; other\r\n", 5 },
        { "5\r\nhello\r\n", 5 },
    };
    for (const auto& c : cases) {
        INFO("line " << c.line);
        // Exactly as large as the line, with nothing after it.
        std::vector<char> buff(c.line.begin(), c.line.end());
        size_t out_size = 12345;
        int res = _read_chunk_size(buff.data(), buff.size(), &out_size);
        REQUIRE( res == 1 );
        REQUIRE( out_size == c.expected );
    }
}

TEST_CASE( "_read_chunk_size rejects invalid sizes" ) {
    const std::string cases[] = {
        "-1\r\n",
        "+a\r\n",
        "xyz\r\n",
        "\r\n",
        "\r\n5\r\n",
        " 5\r\n",
        "0x5\r\n",
        "5 6\r\n",
        ";name\r\n",
        // One past SIZE_MAX, and 20 hex digits, too many for 64 bits.
        "1" + std::string(sizeof(size_t) * 2, '0') + "\r\n",
        "123456789abcdef01234\r\n",
        // Control characters, in the size or in an extension.
        std::string("5\0\r\n", 4),
        std::string("5;\0\r\n", 5),
        "5;a\rb\r\n",
    };
    for (const auto& line : cases) {
        INFO("line " << line);
        std::vector<char> buff(line.begin(), line.end());
        size_t out_size = 12345;
        int res = _read_chunk_size(buff.data(), buff.size(), &out_size);
        REQUIRE( res == -1 );
        REQUIRE( out_size == 12345 );
    }
}

namespace {

bool status_line(const std::string& s) {
    return _is_status_line(s.data(), s.size());
}

bool header_line(const std::string& s) {
    return _is_header_line(s.data(), s.size());
}

bool header_start(const std::string& s) {
    return _is_header_start(s.data(), s.size());
}

bool field_text(const std::string& s) {
    return _is_field_text(s.data(), s.size());
}

// tchar of RFC 9110, section 5.6.2, written out.
bool is_tchar(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        std::string("!#$%&'*+-.^_`|~").find((char)c) != std::string::npos;
}

}

TEST_CASE( "_is_field_text takes HTAB, SP, visible ASCII and bytes from 0x80 up" ) {
    for (int c = 0; c < 256; ++c) {
        INFO( "byte " << c );
        const bool expected = c == '\t' || (c >= 0x20 && c != 0x7f);
        CHECK( field_text(std::string(1, (char)c)) == expected );
        CHECK( field_text("a" + std::string(1, (char)c) + "b") == expected );
    }
    CHECK( field_text("") );
    CHECK( _is_field_text(NULL, 0) );
}

TEST_CASE( "_is_header_line takes a name of tchar only, then a colon" ) {
    for (int c = 0; c < 256; ++c) {
        INFO( "byte " << c );
        CHECK( header_line(std::string(1, (char)c) + ": v") == is_tchar(c) );
        // A colon there ends the name "X" instead.
        CHECK( header_line("X" + std::string(1, (char)c) + "A: v") == (is_tchar(c) || c == ':') );
        // As the value, anything _is_field_text takes.
        CHECK( header_line("X-A: " + std::string(1, (char)c)) == field_text(std::string(1, (char)c)) );
    }
    CHECK( header_line("X-A:") );
    CHECK( header_line("X-A: a:b") );
    CHECK_FALSE( header_line("") );
    CHECK_FALSE( header_line(":") );
    CHECK_FALSE( header_line("X-A") );
    CHECK_FALSE( header_line("X-A ") );
    CHECK_FALSE( header_line("X-A :v") );
    CHECK_FALSE( header_line(" X-A: v") );
    CHECK_FALSE( _is_header_line(NULL, 0) );
}

TEST_CASE( "_is_header_start takes what there is of a header, with a CR last" ) {
    CHECK( header_start("X-A:") );
    CHECK( header_start("X-A: v") );
    CHECK( header_start("X-A: v\r") );
    CHECK_FALSE( header_start("X-A: v\r\r") );
    CHECK_FALSE( header_start("X-A: v\rx") );
    CHECK_FALSE( header_start("X-A: v\n") );
    // The name and its colon have to fit.
    CHECK_FALSE( header_start("X-Long-Name") );
    CHECK_FALSE( header_start("X-Long-Name\r") );
    CHECK_FALSE( header_start("X A: v") );
    CHECK_FALSE( header_start("") );
    CHECK_FALSE( header_start("\r") );
}

TEST_CASE( "_is_status_line takes HTTP/1.x, three digits and a reason phrase" ) {
    CHECK( status_line("HTTP/1.1 200 OK") );
    CHECK( status_line("HTTP/1.0 200 OK") );
    CHECK( status_line("HTTP/1.1 200") );
    CHECK( status_line("HTTP/1.1 200 ") );
    CHECK( status_line("HTTP/1.1 000 X") );
    CHECK( status_line("HTTP/1.1 999 X") );
    for (int c = '0'; c <= '9'; ++c) {
        CHECK( status_line("HTTP/1." + std::string(1, (char)c) + " 200 OK") );
    }
    for (int c = 0; c < 256; ++c) {
        INFO( "byte " << c );
        const std::string b(1, (char)c);
        CHECK( status_line("HTTP/1.1 200 " + b) == field_text(b) );
        CHECK( status_line("HTTP/1.1 200 O" + b + "K") == field_text(b) );
        CHECK( status_line("HTTP/1.1 20" + b + " OK") == (c >= '0' && c <= '9') );
        CHECK( status_line("HTTP/1." + b + " 200 OK") == (c >= '0' && c <= '9') );
        CHECK( status_line("HTTP/1.1 200" + b + "OK") == (c == ' ') );
        CHECK( status_line("HTTP/1.1" + b + "200 OK") == (c == ' ') );
        CHECK( status_line("HTTP/" + b + ".1 200 OK") == (c == '1') );
    }
    CHECK_FALSE( status_line("") );
    CHECK_FALSE( status_line("HTTP/1.1") );
    CHECK_FALSE( status_line("HTTP/1.1 ") );
    CHECK_FALSE( status_line("HTTP/1.1 20") );
    CHECK_FALSE( status_line("HTTP/1.1 2000") );
    CHECK_FALSE( status_line("HTTP/1.10 200 OK") );
    CHECK_FALSE( status_line("HTTP/2.0 200 OK") );
    CHECK_FALSE( status_line("http/1.1 200 OK") );
    CHECK_FALSE( _is_status_line(NULL, 0) );
}
