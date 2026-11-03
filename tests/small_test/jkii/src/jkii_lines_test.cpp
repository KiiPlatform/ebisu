#include <catch.hpp>
#include <jkii.h>
#include <jkii_lines.h>
#include <jkii_writer.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

// What a reader with a buffer of buff_size gives for data fed in pieces of
// piece bytes, or whole, then finished: "R:" before a record, "T:" before a
// truncated one, "LONG" for one too long.
std::vector<std::string> read_all(const std::string& data, size_t buff_size, size_t piece = 0)
{
    std::vector<char> buff(buff_size);
    jkii_lines_t r;
    jkii_lines_init(&r, buff.data(), buff.size());
    std::vector<std::string> got;
    size_t pos = 0;
    while (pos < data.size()) {
        size_t n = piece == 0 ? data.size() - pos : std::min(piece, data.size() - pos);
        const char* chunk = data.data() + pos;
        size_t left = n;
        while (left > 0) {
            size_t used = 0;
            const char* line = nullptr;
            size_t line_len = 0;
            jkii_lines_err_t res = jkii_lines_feed(&r, chunk, left, &used, &line, &line_len);
            REQUIRE( used <= left );
            REQUIRE( (used > 0 || res != JKII_LINES_NEED_MORE) );
            if (res == JKII_LINES_RECORD) {
                REQUIRE( line != nullptr );
                REQUIRE( line[line_len] == '\0' );
                REQUIRE( strlen(line) == line_len );
                got.push_back("R:" + std::string(line, line_len));
            } else if (res == JKII_LINES_TOO_LONG) {
                got.push_back("LONG");
            } else {
                REQUIRE( res == JKII_LINES_NEED_MORE );
                REQUIRE( used == left );
            }
            chunk += used;
            left -= used;
        }
        pos += n;
    }
    const char* line = nullptr;
    size_t line_len = 0;
    jkii_lines_err_t res = jkii_lines_finish(&r, &line, &line_len);
    if (res == JKII_LINES_TRUNCATED) {
        REQUIRE( line[line_len] == '\0' );
        got.push_back("T:" + std::string(line, line_len));
    } else if (res == JKII_LINES_TOO_LONG) {
        got.push_back("LONG");
    } else {
        REQUIRE( res == JKII_LINES_END );
    }
    return got;
}

typedef std::vector<std::string> Records;

}

TEST_CASE( "JSON Lines are read a record at a time" ) {
    const std::string data = "{\"a\":1}\n[2,3]\n\"four\"\n";
    CHECK( read_all(data, 64) == Records({ "R:{\"a\":1}", "R:[2,3]", "R:\"four\"" }) );

    // Each record parses on its own.
    std::vector<char> buff(64);
    jkii_lines_t r;
    jkii_lines_init(&r, buff.data(), buff.size());
    size_t used = 0;
    const char* line = nullptr;
    size_t line_len = 0;
    REQUIRE( jkii_lines_feed(&r, data.data(), data.size(), &used, &line, &line_len) == JKII_LINES_RECORD );
    CHECK( used == strlen("{\"a\":1}\n") );
    jkii_field_t fields[2];
    memset(fields, 0, sizeof(fields));
    fields[0].name = "a";
    fields[0].type = JKII_FIELD_TYPE_INTEGER;
    CHECK( jkii_parse(line, line_len, fields) == JKII_ERR_OK );
    CHECK( fields[0].field_copy.int_value == 1 );
}

TEST_CASE( "JSON Lines fed in pieces give what they give fed whole" ) {
    const std::string data =
        "{\"a\":\"x\\ny\"}\r\n\n   \n\t\r\n[1]\n" + std::string(40, 'z') + "\n{}\n\"last";
    const Records whole = read_all(data, 16);
    CHECK( whole == Records({ "R:{\"a\":\"x\\ny\"}", "R:[1]", "LONG", "R:{}", "T:\"last" }) );
    for (size_t piece = 1; piece <= data.size(); ++piece) {
        INFO( "pieces of " << piece );
        CHECK( read_all(data, 16, piece) == whole );
    }
}

TEST_CASE( "JSON Lines skip what isn't a record" ) {
    // Empty lines and whitespace, with \r\n or \n.
    CHECK( read_all("\n\r\n \t \n", 16).empty() );
    CHECK( read_all("1\r\n\r\n2\n", 16) == Records({ "R:1", "R:2" }) );
    CHECK( read_all("", 16).empty() );
}

TEST_CASE( "A JSON Lines record longer than the buffer is skipped, and the next read" ) {
    // A buffer of 8 holds a record of 7.
    CHECK( read_all("1234567\n", 8) == Records({ "R:1234567" }) );
    CHECK( read_all("12345678\n9\n", 8) == Records({ "LONG", "R:9" }) );
    // At the end, with no '\n' after it.
    CHECK( read_all("1\n12345678", 8) == Records({ "R:1", "LONG" }) );
    // A buffer too small for anything but blank lines.
    CHECK( read_all("1\n\n2\n", 1) == Records({ "LONG", "LONG" }) );
    CHECK( read_all("1\n", 0) == Records({ "LONG" }) );
}

TEST_CASE( "A JSON Lines record cut short by the end of the data comes back as such" ) {
    CHECK( read_all("{\"a\":1}\n{\"b\"", 32) == Records({ "R:{\"a\":1}", "T:{\"b\"" }) );
    CHECK( read_all("{\"a\":1}\n  ", 32) == Records({ "R:{\"a\":1}" }) );
    CHECK( read_all("x\r", 32) == Records({ "T:x" }) );
}

// What the writer writes as JSON Lines, appended a record at a time, the reader
// gives back, after a record cut short and the '\n' written before the next.
TEST_CASE( "JSON Lines written a record at a time read back" ) {
    std::string file;
    auto append = [&file](int seq, const char* note) {
        char line[64];
        size_t len = 0;
        jkii_writer_t w;
        jkii_writer_init_buff(&w, line, sizeof(line));
        jkii_write_object_start(&w);
        jkii_write_key(&w, "seq");
        jkii_write_int(&w, seq);
        jkii_write_key(&w, "note");
        jkii_write_string(&w, note);
        jkii_write_object_end(&w);
        jkii_write_record_end(&w);
        REQUIRE( jkii_writer_finish(&w, &len) == JKII_WRITE_OK );
        file.append(line, len);
    };
    append(0, "a\nb");
    append(1, "c");
    // The next write is cut short, and the one after finds the file not ending
    // with '\n'.
    file += "{\"seq\":2,\"no";
    if (file.back() != '\n') {
        file += '\n';
    }
    append(3, "d");

    CHECK( read_all(file, 64) == Records({
        "R:{\"seq\":0,\"note\":\"a\\nb\"}",
        "R:{\"seq\":1,\"note\":\"c\"}",
        "R:{\"seq\":2,\"no",
        "R:{\"seq\":3,\"note\":\"d\"}" }) );
}
