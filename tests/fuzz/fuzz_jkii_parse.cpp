// A JSON document, asked for a field of each type by a name and a path the
// input chooses: a line of them, then the document. Its root's members are
// walked too.
#include <jkii.h>
#include <jkii_utils.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include "fuzz_input.h"

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
    }
    jkii_validate_root_object(begin, document.size());

    std::vector<char> escaped(key.size() * 2 + 1);
    jkii_escape_str(key.c_str(), escaped.data(), escaped.size());
    return 0;
}
