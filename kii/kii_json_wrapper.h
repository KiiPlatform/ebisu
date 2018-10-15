#ifndef KII_JSON_UTILS_H
#define KII_JSON_UTILS_H

#include "kii.h"

#ifdef __cplusplus
extern "C" {
#endif

kii_json_parse_result_t _kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields);

#ifdef __cplusplus
}
#endif

#endif
