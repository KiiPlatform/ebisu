// A JSON document, asked for a field of each type by a name and a path the
// input chooses: a line of them, then the document. Its root's members are
// walked too, and their names and strings unescaped. The name, escaped, has
// to unescape to itself. Its root is read as a value of any type too.
#include <jkii.h>
#include <jkii_utils.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

// Characters of a string in the document, unescaped into a buffer of their
// length and the terminator, which always suffices: either they are refused,
// or what comes out is no longer and is a string of that length.
static void unescape_fits(const char* s, size_t len) {
    std::vector<char> out(len + 1);
    int out_len = jkii_unescape_str(s, len, out.data(), out.size());
    if (out_len < -1 || out_len > (int)len) abort();
    if (out_len == -1 ? out[0] != '\0' : strlen(out.data()) != (size_t)out_len) abort();
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
            jkii_parse(document.data(), document.size(), fields);
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
        }
        if (member.type == JKII_FIELD_TYPE_STRING) {
            unescape_fits(member.value, member.value_length);
        }
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
