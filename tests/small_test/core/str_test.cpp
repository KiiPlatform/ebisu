#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <ebisu_str.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

// A copy of s in memory of exactly its length, with no NUL after it, so that
// a read past the end is one under ASan.
struct Exact {
    std::vector<char> bytes;
    explicit Exact(const std::string& s) : bytes(s.begin(), s.end()) {}
    const char* ptr() const { return bytes.empty() ? nullptr : bytes.data(); }
    size_t len() const { return bytes.size(); }
};

ebisu_parse_err_t uint_of(const std::string& s, uint64_t max, uint64_t* out) {
    Exact e(s);
    return ebisu_parse_uint(e.ptr(), e.len(), max, out);
}

ebisu_parse_err_t int_of(const std::string& s, int64_t min, int64_t max, int64_t* out) {
    Exact e(s);
    return ebisu_parse_int(e.ptr(), e.len(), min, max, out);
}

ebisu_parse_err_t hex_of(const std::string& s, uint64_t max, uint64_t* out) {
    Exact e(s);
    return ebisu_parse_hex(e.ptr(), e.len(), max, out);
}

ebisu_parse_err_t fixed_of(const std::string& s, unsigned int decimals, int64_t* out,
        int64_t min = INT64_MIN, int64_t max = INT64_MAX) {
    Exact e(s);
    return ebisu_parse_fixed(e.ptr(), e.len(), decimals, min, max, out);
}

std::string span_str(ebisu_span_t s) {
    return std::string(s.ptr, s.len);
}

std::vector<std::string> split(const std::string& s, char delim) {
    Exact e(s);
    ebisu_span_t rest = { e.ptr() != nullptr ? e.ptr() : "", e.len() };
    ebisu_span_t field;
    std::vector<std::string> fields;
    while (ebisu_span_next(&rest, delim, &field)) {
        fields.push_back(span_str(field));
    }
    // Used up, and stays so.
    REQUIRE(rest.ptr == nullptr);
    REQUIRE_FALSE(ebisu_span_next(&rest, delim, &field));
    return fields;
}

} // namespace

TEST_CASE("ebisu_strlcpy copies what fits, terminated, and returns the length") {
    char buff[8];
    memset(buff, '#', sizeof(buff));
    CHECK(ebisu_strlcpy(buff, "abc", sizeof(buff)) == 3);
    CHECK(std::string(buff) == "abc");

    memset(buff, '#', sizeof(buff));
    CHECK(ebisu_strlcpy(buff, "abcdefghij", 4) == 10);
    CHECK(std::string(buff) == "abc");
    CHECK(buff[4] == '#');

    SECTION("exactly one byte short, and exactly fitting") {
        CHECK(ebisu_strlcpy(buff, "abcd", 4) == 4);
        CHECK(std::string(buff) == "abc");
        CHECK(ebisu_strlcpy(buff, "abc", 4) == 3);
        CHECK(std::string(buff) == "abc");
    }
    SECTION("size 1 holds the NUL only, size 0 nothing") {
        CHECK(ebisu_strlcpy(buff, "abc", 1) == 3);
        CHECK(buff[0] == '\0');
        CHECK(ebisu_strlcpy(nullptr, "abc", 0) == 3);
    }
    SECTION("NULL is the empty string") {
        CHECK(ebisu_strlcpy(buff, nullptr, sizeof(buff)) == 0);
        CHECK(buff[0] == '\0');
    }
}

TEST_CASE("ebisu_strbuf_t appends whole, or fails and stays failed") {
    char buff[10];
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, buff, sizeof(buff));
    CHECK(std::string(buff) == "");
    ebisu_strbuf_append(&sb, "id=");
    ebisu_strbuf_append_uint(&sb, 42);
    ebisu_strbuf_append_char(&sb, ';');
    CHECK(ebisu_strbuf_ok(&sb));
    CHECK(std::string(buff) == "id=42;");
    CHECK(sb.len == 6);

    SECTION("an append that doesn't fit appends nothing, nor do later ones") {
        ebisu_strbuf_append(&sb, "abcd");  // 6 + 4 = 10, no room for the NUL
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "id=42;");
        ebisu_strbuf_append(&sb, "x");
        ebisu_strbuf_append_char(&sb, 'y');
        CHECK(std::string(buff) == "id=42;");
        CHECK(sb.len == 6);
    }
    SECTION("filling it exactly is fine") {
        ebisu_strbuf_append(&sb, "abc");
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "id=42;abc");
        ebisu_strbuf_append_n(&sb, "", 0);
        CHECK(ebisu_strbuf_ok(&sb));
        ebisu_strbuf_append_char(&sb, 'd');
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
    }
    SECTION("append_n takes bytes without a NUL") {
        Exact e("xyz");
        ebisu_strbuf_append_n(&sb, e.ptr(), 2);
        CHECK(std::string(buff) == "id=42;xy");
    }
    SECTION("NULL appends nothing") {
        ebisu_strbuf_append(&sb, nullptr);
        ebisu_strbuf_append_n(&sb, nullptr, 0);
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "id=42;");
    }
}

TEST_CASE("ebisu_strbuf_appendf appends strings by a format, whole or not at all") {
    char buff[32];
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, buff, sizeof(buff));
    ebisu_strbuf_append(&sb, "GET ");

    SECTION("%s, NULL taken as \"\"") {
        // Through a volatile, so that GCC doesn't warn of a null %s, which
        // printf() would take as undefined.
        const char* volatile none = nullptr;
        ebisu_strbuf_appendf(&sb, "/apps/%s/things/%s%s", "app1", "th.1", none);
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET /apps/app1/things/th.1");
        CHECK(sb.len == strlen(buff));
    }
    SECTION("%.*s, as many bytes as said, or fewer at a NUL, or all if negative") {
        Exact name("powerOnXYZ");
        ebisu_strbuf_appendf(&sb, "[%.*s|%.*s|%.*s]", 7, name.ptr(), 9, "ab", -1, "cd");
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET [powerOn|ab|cd]");
    }
    SECTION("%% and text alone") {
        ebisu_strbuf_appendf(&sb, "100%% %s", "done");
        ebisu_strbuf_appendf(&sb, "!");
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET 100% done!");
    }
    SECTION("what doesn't fit appends nothing, though its start would") {
        ebisu_strbuf_appendf(&sb, "/apps/%s/%s", "app1", std::string(30, 'x').c_str());
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET ");
        CHECK(sb.len == 4);
        ebisu_strbuf_appendf(&sb, "%s", "x");
        CHECK(std::string(buff) == "GET ");
    }
    SECTION("filling it exactly is fine") {
        ebisu_strbuf_appendf(&sb, "%s", std::string(27, 'x').c_str());
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(sb.len == 31);
    }
    SECTION("another conversion, which it can't write, appends nothing and fails") {
        ebisu_strbuf_appendf(&sb, "/apps/%s/%d", "app1", 7);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET ");
    }
    SECTION("so does a '%' that ends the format") {
        const char* fmt = "50%";
        ebisu_strbuf_appendf(&sb, fmt, "");
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "GET ");
    }
}

TEST_CASE("ebisu_strbuf_appendf with 0 and 1 byte buffers") {
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, nullptr, 0);
    ebisu_strbuf_appendf(&sb, "%s", "");
    CHECK(ebisu_strbuf_ok(&sb));
    ebisu_strbuf_appendf(&sb, "%s", "a");
    CHECK_FALSE(ebisu_strbuf_ok(&sb));

    char one[1];
    ebisu_strbuf_init(&sb, one, sizeof(one));
    ebisu_strbuf_appendf(&sb, "%s%%", "");
    CHECK_FALSE(ebisu_strbuf_ok(&sb));
    CHECK(one[0] == '\0');
}

TEST_CASE("ebisu_strbuf_t with 0 and 1 byte buffers") {
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, nullptr, 0);
    ebisu_strbuf_append(&sb, "");
    CHECK(ebisu_strbuf_ok(&sb));
    ebisu_strbuf_append_char(&sb, 'a');
    CHECK_FALSE(ebisu_strbuf_ok(&sb));

    char one = '#';
    ebisu_strbuf_init(&sb, &one, 1);
    CHECK(one == '\0');
    ebisu_strbuf_append_char(&sb, 'a');
    CHECK_FALSE(ebisu_strbuf_ok(&sb));
    CHECK(one == '\0');
}

namespace {

// A writer as jkii_escape_str() is one: it writes a terminated string into
// what it is given, and returns its length, or -1 if it didn't fit.
int write_terminated(char* dst, size_t size, const char* s) {
    size_t len = strlen(s);
    if (len >= size) {
        return -1;
    }
    memcpy(dst, s, len + 1);
    return (int)len;
}

// Commits what write_terminated() returned, SIZE_MAX for running out of room.
void commit_written(ebisu_strbuf_t* sb, int written) {
    ebisu_strbuf_commit(sb, written < 0 ? SIZE_MAX : (size_t)written);
}

} // namespace

TEST_CASE("ebisu_strbuf_room and ebisu_strbuf_commit append what a writer wrote") {
    char buff[10];
    memset(buff, '#', sizeof(buff));
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, buff, sizeof(buff));
    ebisu_strbuf_append(&sb, "k=");

    size_t room_size = 0;
    char* room = ebisu_strbuf_room(&sb, &room_size);
    REQUIRE(room == &buff[2]);
    CHECK(room_size == 8);
    commit_written(&sb, write_terminated(room, room_size, "abc"));
    ebisu_strbuf_append_char(&sb, ';');
    CHECK(ebisu_strbuf_ok(&sb));
    CHECK(std::string(buff) == "k=abc;");
    CHECK(sb.len == 6);

    SECTION("filling the room exactly is fine") {
        room = ebisu_strbuf_room(&sb, &room_size);
        CHECK(room_size == 4);
        commit_written(&sb, write_terminated(room, room_size, "xyz"));
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "k=abc;xyz");
    }
    SECTION("a writer that ran out of room fails the builder, and leaves it terminated") {
        room = ebisu_strbuf_room(&sb, &room_size);
        // Scribbles over the room, NUL included, then gives up.
        memset(room, '!', room_size);
        commit_written(&sb, write_terminated(room, 0, "wxyz"));
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "k=abc;");
        CHECK(sb.len == 6);
        // Failed, so there is no room, and nothing more is appended.
        CHECK(ebisu_strbuf_room(&sb, &room_size) == nullptr);
        CHECK(room_size == 0);
        ebisu_strbuf_commit(&sb, 0);
        ebisu_strbuf_append_char(&sb, 'x');
        CHECK(std::string(buff) == "k=abc;");
    }
    SECTION("a count as large as the room can't have had a NUL after it") {
        room = ebisu_strbuf_room(&sb, &room_size);
        memset(room, 'x', room_size);
        ebisu_strbuf_commit(&sb, room_size);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "k=abc;");
    }
    SECTION("committing nothing restores the NUL a writer wrote over") {
        room = ebisu_strbuf_room(&sb, &room_size);
        room[0] = '!';
        ebisu_strbuf_commit(&sb, 0);
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "k=abc;");
    }
    SECTION("asking twice gives the same room") {
        size_t again = 0;
        room = ebisu_strbuf_room(&sb, &room_size);
        CHECK(ebisu_strbuf_room(&sb, &again) == room);
        CHECK(again == room_size);
    }
}

TEST_CASE("ebisu_strbuf_room with 0 and 1 byte buffers") {
    ebisu_strbuf_t sb;
    size_t room_size = 1;
    ebisu_strbuf_init(&sb, nullptr, 0);
    CHECK(ebisu_strbuf_room(&sb, &room_size) == nullptr);
    CHECK(room_size == 0);
    ebisu_strbuf_commit(&sb, 0);
    CHECK(ebisu_strbuf_ok(&sb));
    ebisu_strbuf_commit(&sb, 1);
    CHECK_FALSE(ebisu_strbuf_ok(&sb));

    char one = '#';
    ebisu_strbuf_init(&sb, &one, 1);
    char* room = ebisu_strbuf_room(&sb, &room_size);
    CHECK(room == &one);
    CHECK(room_size == 1);
    commit_written(&sb, write_terminated(room, room_size, ""));
    CHECK(ebisu_strbuf_ok(&sb));
    commit_written(&sb, write_terminated(room, room_size, "a"));
    CHECK_FALSE(ebisu_strbuf_ok(&sb));
    CHECK(one == '\0');
}

TEST_CASE("ebisu_strbuf_t numbers") {
    char buff[64];
    ebisu_strbuf_t sb;
    auto fmt = [&](void (*f)(ebisu_strbuf_t*)) {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        f(&sb);
        REQUIRE(ebisu_strbuf_ok(&sb));
        return std::string(buff);
    };
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_uint(s, 0); }) == "0");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_uint(s, UINT64_MAX); }) == "18446744073709551615");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_int(s, 0); }) == "0");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_int(s, -7); }) == "-7");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_int(s, INT64_MAX); }) == "9223372036854775807");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_int(s, INT64_MIN); }) == "-9223372036854775808");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_hex(s, 0, 0); }) == "0");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_hex(s, 0xbeef, 1); }) == "beef");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_hex(s, 0xbeef, 8); }) == "0000beef");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_hex(s, 0x1f, 40); }) == "000000000000001f");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_hex(s, UINT64_MAX, 0); }) == "ffffffffffffffff");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, 12345, 2); }) == "123.45");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, 5, 2); }) == "0.05");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, -5, 2); }) == "-0.05");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, 0, 3); }) == "0.000");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, 7, 0); }) == "7");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, INT64_MIN, 18); }) == "-9.223372036854775808");
    CHECK(fmt([](ebisu_strbuf_t* s) { ebisu_strbuf_append_fixed(s, INT64_MAX, 1); }) == "922337203685477580.7");

    SECTION("too many decimals fail the builder") {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_fixed(&sb, 1, EBISU_FIXED_MAX_DECIMALS + 1);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(buff) == "");
    }
    SECTION("a number that doesn't fit is not cut short") {
        char small[4];
        ebisu_strbuf_init(&sb, small, sizeof(small));
        ebisu_strbuf_append_int(&sb, -1234);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(small) == "");
    }
}

TEST_CASE("ebisu_parse_uint") {
    uint64_t v = 99;
    CHECK(uint_of("0", UINT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 0);
    CHECK(uint_of("007", UINT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 7);
    CHECK(uint_of("18446744073709551615", UINT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == UINT64_MAX);

    v = 99;
    CHECK(uint_of("", UINT64_MAX, &v) == EBISU_PARSE_EMPTY);
    CHECK(uint_of("18446744073709551616", UINT64_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(uint_of("100000000000000000000000", UINT64_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(uint_of("256", 255, &v) == EBISU_PARSE_RANGE);
    CHECK(uint_of("5", 0, &v) == EBISU_PARSE_RANGE);
    CHECK(uint_of("+1", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(uint_of("-1", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(uint_of(" 1", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(uint_of("1 ", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(uint_of("1a", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(uint_of(std::string("1\0" "2", 3), UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    // Not a number before out of range.
    CHECK(uint_of("99999999999999999999999x", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(v == 99);
    CHECK(uint_of("255", 255, &v) == EBISU_PARSE_OK);
    CHECK(v == 255);

    SECTION("only len bytes are read") {
        CHECK(ebisu_parse_uint("12345", 2, UINT64_MAX, &v) == EBISU_PARSE_OK);
        CHECK(v == 12);
    }
}

TEST_CASE("ebisu_parse_int") {
    int64_t v = 99;
    CHECK(int_of("-42", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == -42);
    CHECK(int_of("+42", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 42);
    CHECK(int_of("-0", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 0);
    CHECK(int_of("9223372036854775807", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == INT64_MAX);
    CHECK(int_of("-9223372036854775808", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == INT64_MIN);

    v = 99;
    CHECK(int_of("", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_EMPTY);
    CHECK(int_of("-", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(int_of("+", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(int_of("--1", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(int_of("1-", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(int_of("9223372036854775808", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(int_of("-9223372036854775809", INT64_MIN, INT64_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(int_of("-1", 0, 10, &v) == EBISU_PARSE_RANGE);
    CHECK(int_of("11", 0, 10, &v) == EBISU_PARSE_RANGE);
    CHECK(v == 99);
    CHECK(int_of("10", 0, 10, &v) == EBISU_PARSE_OK);
    CHECK(v == 10);
}

TEST_CASE("ebisu_parse_hex") {
    uint64_t v = 99;
    CHECK(hex_of("42C80000", UINT32_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 0x42c80000);
    CHECK(hex_of("deadBEEF", UINT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == 0xdeadbeef);
    CHECK(hex_of("ffffffffffffffff", UINT64_MAX, &v) == EBISU_PARSE_OK);
    CHECK(v == UINT64_MAX);
    v = 99;
    CHECK(hex_of("", UINT64_MAX, &v) == EBISU_PARSE_EMPTY);
    CHECK(hex_of("10000000000000000", UINT64_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(hex_of("100000000", UINT32_MAX, &v) == EBISU_PARSE_RANGE);
    CHECK(hex_of("0x10", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(hex_of("g", UINT64_MAX, &v) == EBISU_PARSE_INVALID);
    CHECK(v == 99);
}

TEST_CASE("ebisu_parse_hex_bytes") {
    uint8_t out[4];
    size_t out_len = 99;
    auto bytes_of = [&](const std::string& s, size_t out_size) {
        Exact e(s);
        memset(out, 0x55, sizeof(out));
        out_len = 99;
        return ebisu_parse_hex_bytes(e.ptr(), e.len(), out, out_size, &out_len);
    };
    CHECK(bytes_of("01aB", sizeof(out)) == EBISU_PARSE_OK);
    CHECK(out_len == 2);
    CHECK(out[0] == 0x01);
    CHECK(out[1] == 0xab);
    CHECK(out[2] == 0x55);
    CHECK(bytes_of("00ff7F80", sizeof(out)) == EBISU_PARSE_OK);
    CHECK(out_len == 4);
    CHECK(out[0] == 0x00);
    CHECK(out[1] == 0xff);
    CHECK(out[2] == 0x7f);
    CHECK(out[3] == 0x80);

    // Nothing is written, out_len included, unless it parses.
    CHECK(bytes_of("", sizeof(out)) == EBISU_PARSE_EMPTY);
    CHECK(bytes_of("abc", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(bytes_of("0x01", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(bytes_of("01 02", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(bytes_of("0g", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(bytes_of("0102030405", sizeof(out)) == EBISU_PARSE_RANGE);
    CHECK(bytes_of("01", 0) == EBISU_PARSE_RANGE);
    // A character that isn't a digit is INVALID even where out is too small.
    CHECK(bytes_of("010203040g", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(bytes_of("010203040", sizeof(out)) == EBISU_PARSE_INVALID);
    CHECK(out_len == 99);
    CHECK(out[0] == 0x55);

    // Exactly the room there is.
    CHECK(bytes_of("01020304", sizeof(out)) == EBISU_PARSE_OK);
    CHECK(out_len == 4);
    CHECK(ebisu_parse_hex_bytes("01", 2, nullptr, 0, &out_len) == EBISU_PARSE_RANGE);
}

TEST_CASE("ebisu_strbuf_append_hex_bytes") {
    const uint8_t bytes[] = { 0x00, 0x01, 0xab, 0xff };
    char buff[16];
    ebisu_strbuf_t sb;

    ebisu_strbuf_init(&sb, buff, sizeof(buff));
    ebisu_strbuf_append_hex_bytes(&sb, bytes, sizeof(bytes));
    CHECK(ebisu_strbuf_ok(&sb));
    CHECK(std::string(buff) == "0001abff");
    ebisu_strbuf_append_hex_bytes(&sb, nullptr, 0);
    CHECK(ebisu_strbuf_ok(&sb));
    CHECK(sb.len == 8);

    SECTION("exactly the room there is") {
        char small[9];
        ebisu_strbuf_init(&sb, small, sizeof(small));
        ebisu_strbuf_append_hex_bytes(&sb, bytes, sizeof(bytes));
        CHECK(ebisu_strbuf_ok(&sb));
        CHECK(std::string(small) == "0001abff");
    }
    SECTION("bytes that don't fit are not cut short") {
        char small[8];
        ebisu_strbuf_init(&sb, small, sizeof(small));
        ebisu_strbuf_append(&sb, "x");
        ebisu_strbuf_append_hex_bytes(&sb, bytes, sizeof(bytes) - 1);
        CHECK(ebisu_strbuf_ok(&sb));
        ebisu_strbuf_append_hex_bytes(&sb, bytes, 1);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
        CHECK(std::string(small) == "x0001ab");
    }
    SECTION("with no buffer") {
        ebisu_strbuf_init(&sb, nullptr, 0);
        ebisu_strbuf_append_hex_bytes(&sb, bytes, 1);
        CHECK_FALSE(ebisu_strbuf_ok(&sb));
    }

    // And back.
    uint8_t back[4];
    size_t back_len = 0;
    REQUIRE(ebisu_parse_hex_bytes(buff, 8, back, sizeof(back), &back_len) == EBISU_PARSE_OK);
    CHECK(back_len == sizeof(bytes));
    CHECK(memcmp(back, bytes, sizeof(bytes)) == 0);
}

TEST_CASE("ebisu_parse_fixed") {
    int64_t v = 99;
    CHECK(fixed_of("12.5", 2, &v) == EBISU_PARSE_OK);
    CHECK(v == 1250);
    CHECK(fixed_of("12.50", 2, &v) == EBISU_PARSE_OK);
    CHECK(v == 1250);
    CHECK(fixed_of("7", 2, &v) == EBISU_PARSE_OK);
    CHECK(v == 700);
    CHECK(fixed_of("-0.05", 2, &v) == EBISU_PARSE_OK);
    CHECK(v == -5);
    CHECK(fixed_of("+3.1", 3, &v) == EBISU_PARSE_OK);
    CHECK(v == 3100);
    CHECK(fixed_of("0007.0", 0 + 1, &v) == EBISU_PARSE_OK);
    CHECK(v == 70);
    CHECK(fixed_of("42", 0, &v) == EBISU_PARSE_OK);
    CHECK(v == 42);
    CHECK(fixed_of("922337203685477580.7", 1, &v) == EBISU_PARSE_OK);
    CHECK(v == INT64_MAX);
    CHECK(fixed_of("-922337203685477580.8", 1, &v) == EBISU_PARSE_OK);
    CHECK(v == INT64_MIN);
    CHECK(fixed_of("9.223372036854775807", 18, &v) == EBISU_PARSE_OK);
    CHECK(v == INT64_MAX);

    v = 99;
    CHECK(fixed_of("", 2, &v) == EBISU_PARSE_EMPTY);
    CHECK(fixed_of("-", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of(".5", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("5.", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("-.5", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of(".", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("1.2.3", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("1,5", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("1.5e3", 2, &v) == EBISU_PARSE_INVALID);
    // A place more than asked for would have to be rounded away.
    CHECK(fixed_of("1.234", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("1.5", 0, &v) == EBISU_PARSE_INVALID);
    CHECK(fixed_of("922337203685477580.8", 1, &v) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("-922337203685477580.9", 1, &v) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("99999999999999999999", 0, &v) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("10", 18, &v) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("1", EBISU_FIXED_MAX_DECIMALS + 1, &v) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("1.01", 2, &v, 0, 100) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("-0.01", 2, &v, 0, 100) == EBISU_PARSE_RANGE);
    CHECK(fixed_of("x.5", 2, &v) == EBISU_PARSE_INVALID);
    CHECK(v == 99);
    CHECK(fixed_of("1.00", 2, &v, 0, 100) == EBISU_PARSE_OK);
    CHECK(v == 100);
}

TEST_CASE("Formatting then parsing gives the number back") {
    const int64_t values[] = { 0, 1, -1, 9, 10, -10, 99, 12345, -12345,
            INT64_MAX, INT64_MIN, INT64_MAX - 1, INT64_MIN + 1 };
    char buff[64];
    ebisu_strbuf_t sb;
    for (int64_t value : values) {
        for (unsigned int decimals = 0; decimals <= EBISU_FIXED_MAX_DECIMALS; ++decimals) {
            ebisu_strbuf_init(&sb, buff, sizeof(buff));
            ebisu_strbuf_append_fixed(&sb, value, decimals);
            REQUIRE(ebisu_strbuf_ok(&sb));
            int64_t back = 0;
            REQUIRE(fixed_of(buff, decimals, &back) == EBISU_PARSE_OK);
            REQUIRE(back == value);
        }
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_int(&sb, value);
        int64_t back = 0;
        REQUIRE(int_of(buff, INT64_MIN, INT64_MAX, &back) == EBISU_PARSE_OK);
        REQUIRE(back == value);
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_hex(&sb, (uint64_t)value, 0);
        uint64_t uback = 0;
        REQUIRE(hex_of(buff, UINT64_MAX, &uback) == EBISU_PARSE_OK);
        REQUIRE(uback == (uint64_t)value);
    }
}

TEST_CASE("ebisu_span_next splits on a delimiter") {
    CHECK(split("a,b,c", ',') == std::vector<std::string>({ "a", "b", "c" }));
    CHECK(split("a,", ',') == std::vector<std::string>({ "a", "" }));
    CHECK(split(",a", ',') == std::vector<std::string>({ "", "a" }));
    CHECK(split(",,", ',') == std::vector<std::string>({ "", "", "" }));
    CHECK(split("", ',') == std::vector<std::string>({ "" }));
    CHECK(split("abc", ',') == std::vector<std::string>({ "abc" }));
    CHECK(split(std::string("a\0b", 3), '\0') == std::vector<std::string>({ "a", "b" }));

    SECTION("a used-up span leaves the field alone") {
        ebisu_span_t rest = { nullptr, 0 };
        ebisu_span_t field = { "x", 1 };
        CHECK_FALSE(ebisu_span_next(&rest, ',', &field));
        CHECK(span_str(field) == "x");
    }
}

TEST_CASE("ebisu_span_trim, ebisu_span_eq and ebisu_span_from_str") {
    ebisu_span_t s = ebisu_span_from_str(" \t ab c\r\n");
    CHECK(s.len == 9);
    CHECK(span_str(ebisu_span_trim(s)) == "ab c");
    CHECK(ebisu_span_trim(ebisu_span_from_str(" \r\n\t ")).len == 0);
    CHECK(ebisu_span_trim(ebisu_span_from_str("")).len == 0);
    CHECK(ebisu_span_from_str(nullptr).len == 0);

    CHECK(ebisu_span_eq(ebisu_span_trim(s), "ab c"));
    CHECK_FALSE(ebisu_span_eq(ebisu_span_trim(s), "ab"));
    CHECK_FALSE(ebisu_span_eq(ebisu_span_trim(s), "ab c "));
    ebisu_span_t empty = { nullptr, 0 };
    CHECK(ebisu_span_eq(empty, ""));
    CHECK(ebisu_span_eq(empty, nullptr));
    CHECK_FALSE(ebisu_span_eq(empty, "a"));
}

namespace {

bool eq_nocase(const std::string& s, const char* literal) {
    Exact e(s);
    ebisu_span_t span = { e.ptr(), e.len() };
    return ebisu_span_eq_nocase(span, literal);
}

// Where needle is in s, -1 if it isn't, with s in memory of exactly its
// length.
long find(const std::string& s, const char* needle) {
    Exact e(s);
    ebisu_span_t span = { e.ptr(), e.len() };
    size_t at = 12345;
    if (!ebisu_span_find(span, needle, &at)) {
        REQUIRE(at == 12345);  // Left alone.
        return -1;
    }
    return (long)at;
}

} // namespace

TEST_CASE("ebisu_span_eq_nocase folds ASCII letters only") {
    CHECK(eq_nocase("Content-Length", "content-length"));
    CHECK(eq_nocase("CONTENT-LENGTH", "Content-Length"));
    CHECK(eq_nocase("chunked", "CHUNKED"));
    CHECK_FALSE(eq_nocase("Content-Length", "Content-Lengths"));
    CHECK_FALSE(eq_nocase("Content-Length", "Content_Length"));
    // '[' and '{', '@' and '`' differ by the bit that tells case for letters.
    CHECK_FALSE(eq_nocase("[", "{"));
    CHECK_FALSE(eq_nocase("@", "`"));
    // Nor are bytes past ASCII folded, as some locales would.
    CHECK_FALSE(eq_nocase("\xc0", "\xe0"));
    CHECK(eq_nocase("\xc0", "\xc0"));
    CHECK(eq_nocase("", ""));
    CHECK(eq_nocase("", nullptr));
    CHECK_FALSE(eq_nocase("a", nullptr));
}

TEST_CASE("ebisu_span_find finds a string within the span alone") {
    CHECK(find("HTTP/1.1 200 OK\r\nDate", "\r\n") == 15);
    CHECK(find("\r\n", "\r\n") == 0);
    CHECK(find("ab\r\n", "\r\n") == 2);
    CHECK(find("ab\r", "\r\n") == -1);
    CHECK(find("a", "ab") == -1);
    CHECK(find("", "a") == -1);
    // A partial match before the whole one.
    CHECK(find("\r\r\n", "\r\n") == 1);
    CHECK(find("aaab", "aab") == 1);

    SECTION("a NUL is a byte like any other, where strstr() would stop") {
        CHECK(find(std::string("a\0b\r\n", 5), "\r\n") == 3);
        CHECK(find(std::string("a\0b", 3), "b") == 2);
    }
    SECTION("the empty string is at 0") {
        CHECK(find("abc", "") == 0);
        CHECK(find("abc", nullptr) == 0);
        ebisu_span_t empty = { nullptr, 0 };
        size_t at = 7;
        CHECK(ebisu_span_find(empty, "", &at));
        CHECK(at == 0);
        CHECK_FALSE(ebisu_span_find(empty, "a", &at));
    }
}
