#include "kii.h"
#include "kii_json_utils.h"

#include <string.h>

kii_json_parse_result_t prv_kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields)
{
    kii_json_parse_result_t res = KII_JSON_PARSE_INVALID_INPUT;
    kii_json_resource_t* resource = kii->_json_resource;
    if (resource == NULL) {
        res = kii_json_parse_with_allocator(json_string, json_string_size, fields, kii->_json_alloc_cb, kii->_json_free_cb);
    } else {
        res = kii_json_parse(json_string, json_string_size, fields, kii->_json_resource);
    }

    return res;
}

int kii_escape_str(const char* str, char* buff, size_t buff_size) {
    size_t str_len = strlen(str);
    if (buff_size < str_len + 1) {
        return -1;
    }
    size_t escaped_len = str_len;
    for (int i = 0, pos = 0; i < str_len; ++i, ++pos) {
        char c = str[i];
        // FIXME: Ignoriging \u{4 hex}
        if (c == '"' ||
            c == '/' ||
            c == '\\' ||
            c == '\b' ||
            c == '\f' ||
            c == '\n' ||
            c == '\r' ||
            c == '\t')
        {
            ++escaped_len;
            if (buff_size < escaped_len + 1) {
                return - 1;
            }
            buff[pos] = '\\';
            buff[++pos] = c;
        } else {
            buff[pos] = c;
        }
    }
    buff[escaped_len] = '\0';
    return escaped_len;
}
