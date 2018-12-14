#include <string.h>
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
