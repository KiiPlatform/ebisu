#include "catch.hpp"
#include "khc_state_impl.h"

#include <stdint.h>
#include <string>

TEST_CASE( "_khc_chunk_size_line writes the size in hex and CRLF" ) {
    char line[KHC_CHUNK_SIZE_LINE_SIZE];

    const struct {
        size_t size;
        const char* expected;
    } cases[] = {
        { 0, "0\r\n" },
        { 1, "1\r\n" },
        { 15, "f\r\n" },
        { 16, "10\r\n" },
        { 1023, "3ff\r\n" },
        { 1024, "400\r\n" },
        { 0xabcdef, "abcdef\r\n" },
    };
    for (const auto& c : cases) {
        INFO("size " << c.size);
        size_t len = _khc_chunk_size_line(c.size, line);
        REQUIRE( std::string(line) == c.expected );
        REQUIRE( len == std::string(c.expected).size() );
    }

    // The largest size_t, whatever its width, fills the buffer exactly.
    size_t len = _khc_chunk_size_line(SIZE_MAX, line);
    REQUIRE( len == KHC_CHUNK_SIZE_LINE_SIZE - 1 );
    REQUIRE( std::string(line) == std::string(sizeof(size_t) * 2, 'f') + "\r\n" );
}
