// Bytes read as JSON Lines, with a buffer the input sizes, fed whole and then
// in pieces of sizes the input chooses: the two have to give the same records,
// none blank, none with a '\n', and each shorter than the buffer.
#include <jkii_lines.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

namespace {

struct Reading {
    std::vector<int> results;
    std::vector<std::string> lines;
};

void check_line(const char* line, size_t len, size_t buff_size) {
    if (line == nullptr || len >= buff_size || line[len] != '\0') abort();
    if (memchr(line, '\n', len) != nullptr) abort();
    bool blank = true;
    for (size_t i = 0; i < len; ++i) {
        blank = blank && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r');
    }
    if (blank) abort();
}

Reading read(const uint8_t* data, size_t size, size_t buff_size, FuzzInput* pieces) {
    std::vector<char> buff(buff_size);
    jkii_lines_t r;
    jkii_lines_init(&r, buff_size > 0 ? buff.data() : nullptr, buff_size);
    Reading reading;
    size_t pos = 0;
    while (pos < size) {
        size_t n = pieces != nullptr ? pieces->take() % 16 + 1 : size - pos;
        if (n > size - pos) n = size - pos;
        const char* chunk = (const char*)data + pos;
        size_t left = n;
        while (left > 0) {
            size_t used = 0;
            const char* line = nullptr;
            size_t len = 0;
            jkii_lines_err_t res = jkii_lines_feed(&r, chunk, left, &used, &line, &len);
            if (used > left || (used == 0 && res == JKII_LINES_NEED_MORE)) abort();
            if (res == JKII_LINES_NEED_MORE && used != left) abort();
            if (res == JKII_LINES_RECORD) {
                check_line(line, len, buff_size);
                reading.lines.push_back(std::string(line, len));
            }
            if (res != JKII_LINES_NEED_MORE) {
                reading.results.push_back(res);
            }
            chunk += used;
            left -= used;
        }
        pos += n;
    }
    const char* line = nullptr;
    size_t len = 0;
    jkii_lines_err_t res = jkii_lines_finish(&r, &line, &len);
    if (res == JKII_LINES_TRUNCATED) {
        check_line(line, len, buff_size);
        reading.lines.push_back(std::string(line, len));
    }
    reading.results.push_back(res);
    return reading;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput in = { data, size };
    size_t buff_size = in.take() % 64;
    size_t piece_bytes = in.take() % 32;
    if (piece_bytes > in.size) piece_bytes = in.size;
    FuzzInput pieces = { in.data, piece_bytes };
    const uint8_t* text = in.data + piece_bytes;
    size_t text_size = in.size - piece_bytes;

    Reading whole = read(text, text_size, buff_size, nullptr);
    Reading pieced = read(text, text_size, buff_size, &pieces);
    if (whole.results != pieced.results || whole.lines != pieced.lines) abort();
    return 0;
}
