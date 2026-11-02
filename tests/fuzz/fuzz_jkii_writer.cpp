// A sequence of writer calls the input chooses, made four times: into a large
// buffer, into a small one, as a dry run and through a callback. Where the
// large buffer's JSON is finished whole, it has to be JSON, or JSON Lines where
// records were ended, and the others have to agree with it: the same JSON, the
// same length, or the small buffer's JKII_WRITE_ERR_TOO_LARGE with the length
// that was needed.
#include <jkii_writer.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

namespace {

// Strict JSON, as the writer writes it: no whitespace.
struct Validator {
    const std::string& s;
    size_t pos;

    bool value() {
        if (pos >= s.size()) return false;
        char c = s[pos];
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') return string();
        if (c == '-' || (c >= '0' && c <= '9')) return number();
        return literal("true") || literal("false") || literal("null");
    }
    bool literal(const char* l) {
        size_t n = strlen(l);
        if (s.compare(pos, n, l) != 0) return false;
        pos += n;
        return true;
    }
    bool object() {
        ++pos;
        if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
        while (true) {
            if (!string() || pos >= s.size() || s[pos] != ':') return false;
            ++pos;
            if (!value() || pos >= s.size()) return false;
            if (s[pos] == '}') { ++pos; return true; }
            if (s[pos] != ',') return false;
            ++pos;
        }
    }
    bool array() {
        ++pos;
        if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
        while (true) {
            if (!value() || pos >= s.size()) return false;
            if (s[pos] == ']') { ++pos; return true; }
            if (s[pos] != ',') return false;
            ++pos;
        }
    }
    bool string() {
        if (pos >= s.size() || s[pos] != '"') return false;
        ++pos;
        while (pos < s.size()) {
            unsigned char c = (unsigned char)s[pos++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (pos >= s.size()) return false;
                char e = s[pos++];
                if (e == 'u') {
                    for (int i = 0; i < 4; ++i, ++pos) {
                        if (pos >= s.size() || !isxdigit((unsigned char)s[pos])) return false;
                    }
                } else if (!strchr("\"\\/bfnrt", e) || e == '\0') {
                    return false;
                }
            }
        }
        return false;
    }
    bool digits() {
        size_t start = pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
        return pos > start;
    }
    bool number() {
        if (s[pos] == '-') ++pos;
        if (pos < s.size() && s[pos] == '0') {
            ++pos;
        } else if (!digits()) {
            return false;
        }
        if (pos < s.size() && s[pos] == '.') {
            ++pos;
            if (!digits()) return false;
        }
        return true;
    }
};

bool is_json(const std::string& s) {
    Validator v = { s, 0 };
    return v.value() && v.pos == s.size();
}

// One value, or records each ended with '\n' but the last, which may be ended or
// not.
bool is_json_lines(const std::string& s) {
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find('\n', start);
        if (end == std::string::npos) {
            return is_json(s.substr(start));
        }
        if (!is_json(s.substr(start, end - start))) return false;
        start = end + 1;
    }
    return !s.empty();
}

// The calls, the same each time they are made.
void calls(jkii_writer_t* w, FuzzInput in) {
    static const char* raws[] = { "1", "{}", "[]", "\"r\"", "{\"a\":[true,null]}" };
    while (in.size > 0) {
        switch (in.take() % 14) {
            case 0: jkii_write_object_start(w); break;
            case 1: jkii_write_object_end(w); break;
            case 2: jkii_write_array_start(w); break;
            case 3: jkii_write_array_end(w); break;
            case 4:
            case 5:
            case 6: {
                size_t n = in.take() % 16;
                std::string text;
                for (size_t i = 0; i < n && in.size > 0; ++i) text += (char)in.take();
                if (n % 3 == 0) {
                    jkii_write_string_n(w, text.data(), text.size());
                } else if (n % 3 == 1) {
                    jkii_write_string(w, text.c_str());
                } else {
                    jkii_write_key(w, text.c_str());
                }
                break;
            }
            case 7:
            case 8:
            case 9: {
                uint64_t bits = 0;
                for (int i = 0; i < 8; ++i) bits = (bits << 8) | in.take();
                int kind = in.take() % 3;
                if (kind == 0) {
                    jkii_write_int(w, (int64_t)bits);
                } else if (kind == 1) {
                    jkii_write_uint(w, bits);
                } else {
                    double d;
                    memcpy(&d, &bits, sizeof(d));
                    jkii_write_double(w, d, (int)(in.take() % 12) - 1);
                }
                break;
            }
            case 10: jkii_write_bool(w, in.take() & 1); break;
            case 11: jkii_write_null(w); break;
            case 12: {
                const char* raw = raws[in.take() % 5];
                jkii_write_raw(w, raw, strlen(raw));
                break;
            }
            case 13: jkii_write_record_end(w); break;
        }
    }
}

struct Result {
    jkii_write_err_t err;
    size_t len;
    std::string json;
};

Result into_buffer(FuzzInput in, size_t size) {
    std::vector<char> buff(size);
    jkii_writer_t w;
    jkii_writer_init_buff(&w, size > 0 ? buff.data() : nullptr, size);
    calls(&w, in);
    Result r;
    r.err = jkii_writer_finish(&w, &r.len);
    r.json = size > 0 ? buff.data() : "";
    return r;
}

size_t collect(const char* data, size_t size, void* userdata) {
    ((std::string*)userdata)->append(data, size);
    return size;
}

Result through_callback(FuzzInput in, size_t size) {
    std::vector<char> buff(size);
    std::string out;
    jkii_writer_t w;
    jkii_writer_init_cb(&w, buff.data(), size, collect, &out);
    calls(&w, in);
    Result r;
    r.err = jkii_writer_finish(&w, &r.len);
    r.json = out;
    return r;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput in = { data, size };
    size_t small = in.take() % 64 + 1;
    size_t staging = in.take() % 16 + 1;

    Result large = into_buffer(in, 1 << 16);
    Result dry = into_buffer(in, 0);
    Result tight = into_buffer(in, small);
    Result streamed = through_callback(in, staging);

    if (large.err == JKII_WRITE_ERR_TOO_LARGE) {
        return 0; // Not with these calls, at most 16 bytes of text each.
    }
    if (dry.err != large.err || streamed.err != large.err) abort();
    if (tight.err != large.err && !(large.err == JKII_WRITE_OK && tight.err == JKII_WRITE_ERR_TOO_LARGE)) abort();
    if (large.err != JKII_WRITE_OK) {
        return 0;
    }
    if (!is_json_lines(large.json) || large.len != large.json.size()) abort();
    if (dry.len != large.len || streamed.json != large.json || streamed.len != large.len) abort();
    if (tight.err == JKII_WRITE_OK ? tight.json != large.json : tight.len != large.len) abort();
    return 0;
}
