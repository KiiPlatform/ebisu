#include <stdint.h>
#include <string.h>
#include <string>
#include "catch.hpp"
#include <khc.h>
#include "khc_impl.h"

TEST_CASE( "_contains_chunked tests" ) {
    SECTION( "chunked exists" ) {
        const char str[] = "chunked\r\n";
        int res = _contains_chunked(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "ignore case" ) {
        const char str[] = "ChUnkEd\r\n";
        int res = _contains_chunked(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "chunked and others exist" ) {
        const char str[] = "gzip, compress, chunked, deflate\r\n";
        int res = _contains_chunked(str, strlen(str) - 2);
        REQUIRE( res == 1);
    }
    SECTION( "chunked no exist" ) {
        const char str[] = "gzip, compress, deflate\r\n";
        int res = _contains_chunked(str, strlen(str) - 2);
        REQUIRE( res == 0);
    }
    SECTION( "empty string" ) {
        int res = _contains_chunked("", 0);
        REQUIRE( res == 0);
    }
    SECTION( "NULL" ) {
        int res = _contains_chunked(NULL, 0);
        REQUIRE( res == 0);
    }
}

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
    SECTION( "chunked and others exist" ) {
        const char str[] = "Transfer-Encoding: gzip, chunked, compress\r\n";
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
        REQUIRE( res == 1);
        REQUIRE( out_size == 0x1 );
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
