// Bytes the input chooses, parsed by each ebisu_parse_ function from memory of
// exactly their length, so that a read past it is an ASan finding; whatever
// parses has to format back to something that parses to the same value. They
// are split, and each field found again and compared but for case. Then
// a builder with a buffer the input sizes, fed appends the input chooses, some
// written into its room by a writer of their own, and some of bytes escaped:
// it never writes past its buffer, always leaves it terminated, and holds
// exactly the appends that fitted. The same bytes, up to a NUL, are read as a
// level's name, which only the names take, in any case, and whose name reads
// back as the level.
#include <ebisu_log.h>
#include <ebisu_str.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

namespace {

void check_parsers(const std::vector<char>& bytes, unsigned int decimals) {
    const char* s = bytes.empty() ? nullptr : bytes.data();
    size_t len = bytes.size();
    char buff[64];
    ebisu_strbuf_t sb;

    uint64_t u = 0;
    if (ebisu_parse_uint(s, len, UINT64_MAX, &u) == EBISU_PARSE_OK) {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_uint(&sb, u);
        uint64_t back = 0;
        if (!ebisu_strbuf_ok(&sb) || ebisu_parse_uint(buff, sb.len, UINT64_MAX, &back) != EBISU_PARSE_OK || back != u) abort();
    }
    if (ebisu_parse_hex(s, len, UINT64_MAX, &u) == EBISU_PARSE_OK) {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_hex(&sb, u, 0);
        uint64_t back = 0;
        if (!ebisu_strbuf_ok(&sb) || ebisu_parse_hex(buff, sb.len, UINT64_MAX, &back) != EBISU_PARSE_OK || back != u) abort();
    }
    // Bytes, into room for one fewer than half the length and room for all.
    uint8_t parsed[24];
    size_t parsed_len = 0;
    if (len >= 2 && ebisu_parse_hex_bytes(s, len, parsed, len / 2 - 1, &parsed_len) == EBISU_PARSE_OK) abort();
    if (ebisu_parse_hex_bytes(s, len, parsed, sizeof(parsed), &parsed_len) == EBISU_PARSE_OK) {
        if (parsed_len != len / 2) abort();
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_hex_bytes(&sb, parsed, parsed_len);
        if (!ebisu_strbuf_ok(&sb) || sb.len != len) abort();
        for (size_t k = 0; k < len; ++k) {
            char c = s[k] >= 'A' && s[k] <= 'F' ? (char)(s[k] - 'A' + 'a') : s[k];
            if (buff[k] != c) abort();
        }
    }
    int64_t i = 0;
    if (ebisu_parse_int(s, len, INT64_MIN, INT64_MAX, &i) == EBISU_PARSE_OK) {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_int(&sb, i);
        int64_t back = 0;
        if (!ebisu_strbuf_ok(&sb) || ebisu_parse_int(buff, sb.len, INT64_MIN, INT64_MAX, &back) != EBISU_PARSE_OK || back != i) abort();
    }
    if (ebisu_parse_fixed(s, len, decimals, INT64_MIN, INT64_MAX, &i) == EBISU_PARSE_OK) {
        ebisu_strbuf_init(&sb, buff, sizeof(buff));
        ebisu_strbuf_append_fixed(&sb, i, decimals);
        int64_t back = 0;
        if (!ebisu_strbuf_ok(&sb) || ebisu_parse_fixed(buff, sb.len, decimals, INT64_MIN, INT64_MAX, &back) != EBISU_PARSE_OK || back != i) abort();
    }
}

void check_spans(const std::vector<char>& bytes, char delim) {
    ebisu_span_t rest = { bytes.empty() ? "" : bytes.data(), bytes.size() };
    ebisu_span_t field;
    std::string joined;
    size_t fields = 0;
    while (ebisu_span_next(&rest, delim, &field)) {
        if (fields++ > 0) joined += delim;
        joined.append(field.ptr, field.len);
        ebisu_span_t t = ebisu_span_trim(field);
        if (t.len > field.len || t.ptr < field.ptr || t.ptr + t.len > field.ptr + field.len) abort();
    }
    // Splitting and joining back gives what was split.
    const std::string all(bytes.begin(), bytes.end());
    if (joined != all) abort();

    // Finding each field's text, up to a NUL, which ends it as a string, and
    // a delimiter, gives what std::string's find() does.
    ebisu_span_t whole = { bytes.empty() ? "" : bytes.data(), bytes.size() };
    rest = whole;
    while (ebisu_span_next(&rest, delim, &field)) {
        std::string needle(field.ptr, field.len);
        needle = needle.substr(0, needle.find('\0'));
        for (const std::string& n : { needle, needle + delim }) {
            size_t at = 0;
            bool found = ebisu_span_find(whole, n.c_str(), &at);
            size_t expected = all.find(std::string(n.c_str()));
            if (found != (expected != std::string::npos) || (found && at != expected)) abort();
        }
        // A field matches itself in either case, ASCII letters folded.
        if (needle.size() == field.len) {
            std::string other(needle);
            for (char& c : other) {
                if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
                else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            }
            if (!ebisu_span_eq_nocase(field, other.c_str())) abort();
            if (!other.empty()) {
                // And not once a byte that isn't a letter is changed.
                for (size_t i = 0; i < other.size(); ++i) {
                    char c = other[i];
                    bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
                    if (!letter && (char)(c ^ 0x20) != '\0') {
                        std::string changed(other);
                        changed[i] = (char)(c ^ 0x20);
                        if (ebisu_span_eq_nocase(field, changed.c_str())) abort();
                        break;
                    }
                }
            }
        }
    }
}

void check_builder(FuzzInput* in) {
    size_t size = in->take();
    // One guard byte after the buffer, which must stay as it is.
    std::vector<char> mem(size + 1, '#');
    ebisu_strbuf_t sb;
    ebisu_strbuf_init(&sb, size > 0 ? mem.data() : nullptr, size);
    std::string expected;
    bool failed = false;
    while (in->size > 0) {
        uint8_t op = in->take();
        uint8_t n = in->take();
        std::string piece;
        char tmp[64];
        switch (op % 8) {
        case 0: {
            piece.assign((const char*)in->data, n < in->size ? n : in->size);
            in->data += piece.size();
            in->size -= piece.size();
            ebisu_strbuf_append_n(&sb, piece.data(), piece.size());
            break;
        }
        case 4: {
            // A writer into the room, which leaves something other than a NUL
            // after what it wrote, and all over the room when it runs out.
            piece.assign((const char*)in->data, n < in->size ? n : in->size);
            in->data += piece.size();
            in->size -= piece.size();
            size_t room_size = 0;
            char* room = ebisu_strbuf_room(&sb, &room_size);
            if (piece.size() < room_size) {
                memcpy(room, piece.data(), piece.size());
                room[piece.size()] = '!';
                ebisu_strbuf_commit(&sb, piece.size());
            } else {
                if (room_size > 0) memset(room, '!', room_size);
                ebisu_strbuf_commit(&sb, piece.empty() ? 0 : SIZE_MAX);
            }
            break;
        }
        case 1:
            piece.assign(1, (char)n);
            ebisu_strbuf_append_char(&sb, (char)n);
            break;
        case 2:
            snprintf(tmp, sizeof(tmp), "%lld", (long long)((int64_t)n * 1000003 - 100000000));
            piece = tmp;
            ebisu_strbuf_append_int(&sb, (int64_t)n * 1000003 - 100000000);
            break;
        case 3:
            snprintf(tmp, sizeof(tmp), "%llx", (unsigned long long)n * 0x10001ULL);
            piece = tmp;
            ebisu_strbuf_append_hex(&sb, (uint64_t)n * 0x10001ULL, 0);
            break;
        case 5: {
            size_t count = n < in->size ? n : in->size;
            for (size_t i = 0; i < count; ++i) {
                snprintf(tmp, sizeof(tmp), "%02x", in->data[i]);
                piece += tmp;
            }
            ebisu_strbuf_append_hex_bytes(&sb, count > 0 ? in->data : nullptr, count);
            in->data += count;
            in->size -= count;
            break;
        }
        case 7: {
            // Bytes escaped, either way, as an escaper of the fuzzer's own
            // escapes them.
            static const char* const names[33] = {
                "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
                "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
                "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
                "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US", "DEL"
            };
            bool by_name = (n & 0x80) != 0;
            size_t count = (n & 0x7f) < in->size ? (n & 0x7f) : in->size;
            for (size_t i = 0; i < count; ++i) {
                uint8_t c = in->data[i];
                if (by_name && (c < 0x20 || c == 0x7f)) {
                    piece += std::string("<") + names[c < 0x20 ? c : 32] + ">";
                } else if (c == '\\') {
                    piece += "\\\\";
                } else if (c >= 0x20 && c < 0x7f && !(by_name && c == '<')) {
                    piece += (char)c;
                } else {
                    snprintf(tmp, sizeof(tmp), "\\x%02x", c);
                    piece += tmp;
                }
            }
            ebisu_strbuf_append_escaped(&sb, count > 0 ? in->data : nullptr, count,
                    by_name ? EBISU_ESCAPE_CONTROL_NAMES : EBISU_ESCAPE_HEX);
            in->data += count;
            in->size -= count;
            break;
        }
        default:
            snprintf(tmp, sizeof(tmp), "%llu", (unsigned long long)n * 99991ULL);
            piece = tmp;
            ebisu_strbuf_append_uint(&sb, (uint64_t)n * 99991ULL);
            break;
        }
        if (!failed && !piece.empty()) {
            if (size == 0 || expected.size() + piece.size() > size - 1) {
                failed = true;
            } else {
                expected += piece;
            }
        }
    }
    if (mem[size] != '#') abort();
    if (ebisu_strbuf_ok(&sb) == failed) abort();
    if (sb.len != expected.size()) abort();
    if (size > 0 && (mem[sb.len] != '\0' || memcmp(mem.data(), expected.data(), expected.size()) != 0)) abort();
}

void check_level_name(const std::vector<char>& bytes) {
    const std::string name(bytes.begin(), bytes.end());
    // A level as it was, as an enum can't hold anything else.
    ebisu_log_level_t level = EBISU_LOG_INFO;
    bool parsed = ebisu_log_level_parse(name.c_str(), &level);
    // A C string ends at its first NUL.
    std::string lower = name.substr(0, name.find('\0'));
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    }
    bool is_name = lower == "error" || lower == "warn" || lower == "warning" ||
        lower == "info" || lower == "debug" || lower == "trace";
    if (parsed != is_name) abort();
    if (!parsed) {
        if (level != EBISU_LOG_INFO) abort();
        return;
    }
    ebisu_log_level_t back = level == EBISU_LOG_ERROR ? EBISU_LOG_DEBUG : EBISU_LOG_ERROR;
    if (!ebisu_log_level_parse(ebisu_log_level_name(level), &back) || back != level) abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput in = { data, size };
    unsigned int decimals = in.take() % (EBISU_FIXED_MAX_DECIMALS + 1);
    char delim = (char)in.take();
    size_t len = in.take() % 48;
    if (len > in.size) len = in.size;
    std::vector<char> bytes(in.data, in.data + len);
    in.data += len;
    in.size -= len;
    check_parsers(bytes, decimals);
    check_spans(bytes, delim);
    check_level_name(bytes);
    check_builder(&in);
    return 0;
}
