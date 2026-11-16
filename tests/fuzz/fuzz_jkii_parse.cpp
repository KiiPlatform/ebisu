// A JSON document, asked for a field of each type by a name and a path the
// input chooses: a line of them, then the document. Its root's members are
// walked too, and their names and strings unescaped. The name, escaped, has
// to unescape to itself. Its root is read as a value of any type too. The path
// is looked up with jkii_find_member() as well, which has to find what
// jkii_parse() does, and what it finds is read as a string and an integer,
// and by the finding readers, which have to read what the member readers do.
// Each member of the root object is found again by its name, unescaped, with
// jkii_find_name(), and each member is read as text and cut to fit.
#include <jkii.h>
#include <jkii_utils.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

// A member read as a string and as an integer: the string no longer than the
// member's characters, and the integer, if it is one, what jkii_parse_primitive()
// reads too, exactly for an integer as written, and as its double when it is
// below 2^53, which a double holds exactly.
static void read_member(const jkii_member_t& m);

static bool is_json_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// A string is taken as a number, true or false exactly when its characters,
// as a document of their own, are one, with nothing around them; and then is
// one of the type the document's root has, over the same characters.
static void unquote_member(const jkii_member_t& m) {
    jkii_member_t u;
    memset(&u, 0, sizeof(u));
    const jkii_member_t* r = jkii_member_unquote(&m, &u);
    if (m.type != JKII_FIELD_TYPE_STRING) {
        if (r != &m) abort();
        return;
    }
    jkii_member_t root;
    const bool scalar = m.value_length > 0 && !is_json_space(m.value[0]) &&
        !is_json_space(m.value[m.value_length - 1]) &&
        jkii_root_value(m.value, m.value_length, &root) == JKII_ERR_OK &&
        (root.type == JKII_FIELD_TYPE_INTEGER || root.type == JKII_FIELD_TYPE_LONG ||
         root.type == JKII_FIELD_TYPE_DOUBLE || root.type == JKII_FIELD_TYPE_BOOLEAN ||
         root.type == JKII_FIELD_TYPE_ANY);
    if (!scalar) {
        if (r != &m) abort();
        return;
    }
    if (r != &u || u.type != root.type || u.value != m.value ||
            u.value_length != m.value_length || u.key != m.key ||
            u.key_length != m.key_length) abort();
    read_member(u);
}

// As text, and cut to fit: the text a string's, unescaped, and anything
// else's as written, but an object's or an array's; and a string cut is what
// it is whole, or a start of it, cut where a character ends.
static void read_text(const jkii_member_t& m, jkii_field_err_t s, const std::vector<char>& whole) {
    std::vector<char> text(m.value_length + 1);
    jkii_field_err_t t = jkii_member_text(&m, text.data(), text.size());
    if (m.type == JKII_FIELD_TYPE_STRING) {
        if (t != s || strcmp(text.data(), whole.data()) != 0) abort();
    } else if (m.type == JKII_FIELD_TYPE_NULL) {
        if (t != JKII_FIELD_ERR_NULL) abort();
    } else if (m.type == JKII_FIELD_TYPE_OBJECT || m.type == JKII_FIELD_TYPE_ARRAY) {
        if (t != JKII_FIELD_ERR_TYPE_MISMATCH) abort();
    } else if (t != JKII_FIELD_ERR_OK || std::string(text.data()) != std::string(m.value, m.value_length)) {
        abort();
    }
    if (m.type != JKII_FIELD_TYPE_STRING) {
        return;
    }
    for (size_t size : { m.value_length + 1, m.value_length / 2 + 1, (size_t)1, (size_t)4 }) {
        std::vector<char> cut(size, 'x');
        jkii_boolean_t truncated = JKII_FALSE;
        jkii_field_err_t c = jkii_member_string_trunc(&m, cut.data(), cut.size(), &truncated);
        const size_t len = strlen(cut.data());
        if (c == JKII_FIELD_ERR_COPY) {
            // Only for an escape jkii_unescape_str() refuses too.
            if (s == JKII_FIELD_ERR_OK || len != 0) abort();
            continue;
        }
        if (c != JKII_FIELD_ERR_OK || len >= size) abort();
        if (s == JKII_FIELD_ERR_OK) {
            const size_t whole_len = strlen(whole.data());
            if (memcmp(cut.data(), whole.data(), len) != 0) abort();
            if ((truncated == JKII_TRUE) != (len < whole_len)) abort();
            // Cut before a character, not within one.
            if (len < whole_len && ((unsigned char)whole[len] & 0xc0) == 0x80) abort();
        }
    }
}

static void read_member(const jkii_member_t& m) {
    unquote_member(m);
    std::vector<char> buff(m.value_length + 1);
    jkii_field_err_t s = jkii_member_string(&m, buff.data(), buff.size());
    // What each read gives a value that isn't its own: null as itself.
    const jkii_field_err_t other = m.type == JKII_FIELD_TYPE_NULL ?
        JKII_FIELD_ERR_NULL : JKII_FIELD_ERR_TYPE_MISMATCH;
    if (m.type == JKII_FIELD_TYPE_STRING) {
        if (s != JKII_FIELD_ERR_OK && s != JKII_FIELD_ERR_COPY) abort();
        if (strlen(buff.data()) > m.value_length) abort();
    } else if (s != other || buff[0] != '\0') {
        abort();
    }
    read_text(m, s, buff);
    int64_t value = 0;
    jkii_field_err_t i = jkii_member_integer(&m, INT64_MIN, INT64_MAX, &value);
    const bool numeric = m.type == JKII_FIELD_TYPE_INTEGER || m.type == JKII_FIELD_TYPE_LONG ||
        m.type == JKII_FIELD_TYPE_DOUBLE;
    // True or false only, as written.
    jkii_boolean_t b = JKII_FALSE;
    jkii_field_err_t br = jkii_member_boolean(&m, &b);
    if ((br == JKII_FIELD_ERR_OK) != (m.type == JKII_FIELD_TYPE_BOOLEAN)) abort();
    if (br != JKII_FIELD_ERR_OK && br != other) abort();
    if (br == JKII_FIELD_ERR_OK && (b == JKII_TRUE) != (m.value[0] == 't')) abort();
    if (!numeric) {
        double d = 0;
        if (i != other || jkii_member_double(&m, &d) != other) abort();
        return;
    }
    jkii_primitive_t p;
    jkii_primitive_err_t pr = jkii_parse_primitive(m.value, m.value_length, &p);
    if (i == JKII_FIELD_ERR_OK && pr == JKII_PRIMITIVE_ERR_OK) {
        if (p.type == JKII_FIELD_TYPE_INTEGER && value != p.value.int_value) abort();
        if (p.type == JKII_FIELD_TYPE_LONG && value != p.value.long_value) abort();
        if (p.type == JKII_FIELD_TYPE_DOUBLE && value > -(INT64_C(1) << 53) &&
                value < (INT64_C(1) << 53) && (double)value != p.value.double_value) abort();
    }
    // An integer as written is one, whatever its range.
    if ((m.type == JKII_FIELD_TYPE_INTEGER || m.type == JKII_FIELD_TYPE_LONG) &&
            i == JKII_FIELD_ERR_TYPE_MISMATCH) abort();
    // As a double: never NaN nor an infinity, and for an integer as written
    // that int64_t holds, the integer's own double.
    double d = 0;
    jkii_field_err_t dr = jkii_member_double(&m, &d);
    if (dr == JKII_FIELD_ERR_OK) {
        if (!(d == d) || d > DBL_MAX || d < -DBL_MAX) abort();
        if (pr == JKII_PRIMITIVE_ERR_OK && p.type == JKII_FIELD_TYPE_INTEGER &&
                d != (double)p.value.int_value) abort();
        if (pr == JKII_PRIMITIVE_ERR_OK && p.type == JKII_FIELD_TYPE_LONG &&
                d != (double)p.value.long_value) abort();
    } else if (dr != JKII_FIELD_ERR_NUM_OVERFLOW && dr != JKII_FIELD_ERR_NUM_UNDERFLOW &&
            dr != JKII_FIELD_ERR_TYPE_MISMATCH) {
        abort();
    }
}

// Characters of a string in the document, unescaped into a buffer of their
// length and the terminator, which always suffices: either they are refused,
// or what comes out is no longer and is a string of that length.
static void unescape_fits(const char* s, size_t len) {
    std::vector<char> out(len + 1);
    int out_len = jkii_unescape_str(s, len, out.data(), out.size());
    if (out_len < -1 || out_len > (int)len) abort();
    if (out_len == -1 ? out[0] != '\0' : strlen(out.data()) != (size_t)out_len) abort();
}

// The finding readers, at a path that jkii_find_member() gave res and found
// for: each the member reader's result on what it found, or that it found
// nothing, null as absent when asked, and found set whenever there is a value.
static void find_and_read(const std::vector<char>& document, const char* path,
        jkii_parse_err_t res, const jkii_member_t& found) {
    const jkii_field_err_t none = res == JKII_ERR_NOT_FOUND ? JKII_FIELD_ERR_NOT_FOUND :
        JKII_FIELD_ERR_INVALID_INPUT;
    for (unsigned int flags = 0; flags < 8; ++flags) {
        jkii_member_t got;
        memset(&got, 0, sizeof(got));
        int64_t i = 0;
        jkii_field_err_t fi = jkii_find_integer(document.data(), document.size(), path,
                INT64_MIN, INT64_MAX, flags, &i, &got);
        double d = 0;
        jkii_field_err_t fd = jkii_find_double(document.data(), document.size(), path, flags,
                &d, nullptr);
        jkii_boolean_t b = JKII_FALSE;
        jkii_field_err_t fb = jkii_find_boolean(document.data(), document.size(), path, flags,
                &b, nullptr);
        std::vector<char> str(document.size() + 1);
        jkii_field_err_t fs = jkii_find_string(document.data(), document.size(), path, flags,
                str.data(), str.size(), nullptr);
        if (res != JKII_ERR_OK) {
            if (fi != none || fd != none || fb != none || fs != none || got.value != nullptr) abort();
            continue;
        }
        if (got.value != found.value || got.value_length != found.value_length ||
                got.type != found.type) abort();
        if (found.type == JKII_FIELD_TYPE_NULL) {
            const jkii_field_err_t n = (flags & JKII_READ_NULL_AS_ABSENT) != 0 ?
                JKII_FIELD_ERR_NOT_FOUND : JKII_FIELD_ERR_NULL;
            if (fi != n || fd != n || fb != n || fs != n) abort();
            continue;
        }
        jkii_member_t u;
        const jkii_member_t* m = (flags & JKII_READ_UNQUOTE) != 0 ? jkii_member_unquote(&found, &u) : &found;
        int64_t mi = 0;
        jkii_field_err_t ri = jkii_member_integer(m, INT64_MIN, INT64_MAX, &mi);
        if ((flags & JKII_READ_INTEGER_AS_WRITTEN) != 0 && m->type != JKII_FIELD_TYPE_INTEGER &&
                m->type != JKII_FIELD_TYPE_LONG) {
            ri = JKII_FIELD_ERR_TYPE_MISMATCH;
        }
        if (fi != ri || (fi == JKII_FIELD_ERR_OK && i != mi)) abort();
        double md = 0;
        jkii_field_err_t rd = jkii_member_double(m, &md);
        if (fd != rd || (fd == JKII_FIELD_ERR_OK && d != md)) abort();
        jkii_boolean_t mb = JKII_FALSE;
        jkii_field_err_t rb = jkii_member_boolean(m, &mb);
        if (fb != rb || (fb == JKII_FIELD_ERR_OK && b != mb)) abort();
        std::vector<char> ms(document.size() + 1);
        if (fs != jkii_member_string(&found, ms.data(), ms.size()) || strcmp(str.data(), ms.data()) != 0) abort();
    }
}

// A member of the root object, found again by its name unescaped: the first
// whose name unescapes to it, which may be an earlier one of the same name.
static void find_by_name(const std::vector<char>& document, const jkii_member_t& member) {
    std::vector<char> name(member.key_length + 1);
    int len = jkii_unescape_str(member.key, member.key_length, name.data(), name.size());
    if (len < 0) {
        return;
    }
    jkii_member_t found;
    if (jkii_find_name(document.data(), document.size(), name.data(), (size_t)len, &found) !=
            JKII_ERR_OK) abort();
    if (found.key == nullptr || found.key > member.key) abort();
    std::vector<char> found_name(found.key_length + 1);
    int found_len = jkii_unescape_str(found.key, found.key_length, found_name.data(),
            found_name.size());
    if (found_len != len || memcmp(found_name.data(), name.data(), (size_t)len) != 0) abort();
    if (found.key == member.key && (found.value != member.value ||
                found.value_length != member.value_length || found.type != member.type)) abort();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input((const char*)data, size);
    size_t name_end = input.find('\n');
    if (name_end == std::string::npos) {
        return 0;
    }
    const std::string key = input.substr(0, name_end);
    const std::string json = input.substr(name_end + 1);
    if (key.empty() || key.find('\0') != std::string::npos || json.empty()) {
        return 0;
    }
    // Not terminated, as a response in a buffer is not.
    const std::vector<char> document(json.begin(), json.end());

    const jkii_field_type_t types[] = {
        JKII_FIELD_TYPE_ANY, JKII_FIELD_TYPE_INTEGER, JKII_FIELD_TYPE_LONG,
        JKII_FIELD_TYPE_DOUBLE, JKII_FIELD_TYPE_BOOLEAN, JKII_FIELD_TYPE_NULL,
        JKII_FIELD_TYPE_STRING, JKII_FIELD_TYPE_OBJECT, JKII_FIELD_TYPE_ARRAY,
    };
    for (bool by_name : { true, false }) {
        for (jkii_field_type_t type : types) {
            char copy[16];
            jkii_field_t fields[2];
            memset(fields, 0, sizeof(fields));
            if (by_name) {
                fields[0].name = key.c_str();
            } else {
                fields[0].path = key.c_str();
            }
            fields[0].type = type;
            if (type == JKII_FIELD_TYPE_STRING || type == JKII_FIELD_TYPE_OBJECT ||
                    type == JKII_FIELD_TYPE_ARRAY || type == JKII_FIELD_TYPE_ANY) {
                fields[0].field_copy.string = copy;
                fields[0].field_copy_buff_size = sizeof(copy);
            }
            jkii_parse_err_t parsed = jkii_parse(document.data(), document.size(), fields);
            if (!by_name && type == JKII_FIELD_TYPE_ANY) {
                // Found by jkii_find_member() as by jkii_parse(), of the same type.
                jkii_member_t found;
                jkii_parse_err_t res = jkii_find_member(document.data(), document.size(),
                        key.c_str(), &found);
                const bool parse_found = (parsed == JKII_ERR_OK || parsed == JKII_ERR_PARTIAL) &&
                    fields[0].result != JKII_FIELD_ERR_NOT_FOUND;
                if ((res == JKII_ERR_OK) != parse_found) abort();
                if (res == JKII_ERR_OK) {
                    if (found.type != fields[0].type) abort();
                    if (found.value < document.data() ||
                            found.value + found.value_length > document.data() + document.size()) {
                        abort();
                    }
                    read_member(found);
                }
                find_and_read(document, key.c_str(), res, found);
            }
        }
    }

    // Every member of the root, if it is an object or an array: each within the
    // document, and only as many as it could hold.
    const char* begin = document.data();
    const char* end = begin + document.size();
    jkii_member_iter_t iter;
    memset(&iter, 0, sizeof(iter));
    jkii_member_t member;
    size_t members = 0;
    while (jkii_next_member(begin, document.size(), &iter, &member) == JKII_ERR_OK) {
        if (member.value < begin || member.value + member.value_length > end) abort();
        if (member.key != NULL && (member.key < begin || member.key + member.key_length > end)) abort();
        if (++members > document.size()) abort();
        if (member.key != NULL) {
            unescape_fits(member.key, member.key_length);
            find_by_name(document, member);
        }
        if (member.type == JKII_FIELD_TYPE_STRING) {
            unescape_fits(member.value, member.value_length);
        }
        read_member(member);
    }
    jkii_validate_root_object(begin, document.size());

    // The root as a value, of any type: within the document, taken whenever
    // jkii_next_member() takes it, and an object or an array that walks as
    // the document does.
    jkii_member_t root;
    jkii_parse_err_t root_res = jkii_root_value(begin, document.size(), &root);
    memset(&iter, 0, sizeof(iter));
    jkii_parse_err_t first_res = jkii_next_member(begin, document.size(), &iter, &member);
    if ((root_res == JKII_ERR_OK) != (first_res != JKII_ERR_INVALID_INPUT)) abort();
    if (root_res == JKII_ERR_OK) {
        if (root.key != NULL || root.value < begin || root.value + root.value_length > end) abort();
        const bool walks = root.type == JKII_FIELD_TYPE_OBJECT || root.type == JKII_FIELD_TYPE_ARRAY;
        if (walks != (first_res != JKII_ERR_ROOT_TYPE)) abort();
        if (root.type == JKII_FIELD_TYPE_STRING) {
            unescape_fits(root.value, root.value_length);
        }
        read_member(root);
        if (walks) {
            jkii_member_iter_t root_iter;
            memset(&root_iter, 0, sizeof(root_iter));
            jkii_member_t root_member;
            size_t root_members = 0;
            while (jkii_next_member(root.value, root.value_length, &root_iter, &root_member) ==
                    JKII_ERR_OK) {
                ++root_members;
            }
            if (root_members != members) abort();
        }
    }

    // Six times as long, which any escaping fits in, then back.
    std::vector<char> escaped(key.size() * 6 + 1);
    int escaped_len = jkii_escape_str(key.c_str(), escaped.data(), escaped.size());
    if (escaped_len < 0) abort();
    std::vector<char> unescaped(key.size() + 1);
    int unescaped_len = jkii_unescape_str(escaped.data(), (size_t)escaped_len,
            unescaped.data(), unescaped.size());
    if (unescaped_len != (int)key.size() || memcmp(unescaped.data(), key.c_str(), key.size() + 1) != 0) abort();
    return 0;
}
