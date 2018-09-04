#ifndef KII_JSON_UTILS_H
#define KII_JSON_UTILS_H

#include "kii.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_json_parse_result_t prv_kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields);

int kii_escape_str(const char* str, char* buff, size_t buff_size);

#ifdef __cplusplus
}
#endif

#endif
