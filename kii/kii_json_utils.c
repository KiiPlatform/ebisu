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

