#include "kii.h"
#include "kii_json_utils.h"

#include <string.h>

kii_json_parse_result_t prv_kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields)
{
    kii_json_t kii_json;
    kii_json_parse_result_t retval;
    char error_message[50];

    memset(&kii_json, 0, sizeof(kii_json));
    kii_json.error_string_buff = error_message;
    kii_json.error_string_length =
        sizeof(error_message) / sizeof(error_message[0]);

    kii_json.resource = &(kii->kii_json_resource);
    retval = kii_json_read_object(&kii_json, json_string, json_string_size,
                fields);

    if (retval != KII_JSON_PARSE_SUCCESS) {
        M_KII_LOG(kii->kii_core.logger_cb(
                "fail to parse json: result=%d, message=%s\n",
                retval, kii_json.error_string_buff));
    }
    return retval;
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
