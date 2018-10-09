#ifndef _kii_json_utils_
#define _kii_json_utils_

#ifdef __cplusplus
extern "C" {
#endif

#include "kii_json.h"

typedef enum kii_json_util_parse_result_t
{
    KII_JSON_UTIL_PARSE_RESULT_SUCCESS,
    KII_JSON_UTIL_PARSE_RESULT_OVERFLOW,
    KII_JSON_UTIL_PARSE_RESULT_UNDERFLOW,
    KII_JSON_UTIL_PARSE_RESULT_INVALID
} kii_json_util_parse_result_t;

typedef struct kii_json_primitive {
    union {
        long integer_value;
        double double_value;
        kii_json_boolean_t boolean_value;
    } value;
    kii_json_field_type_t type;
} kii_json_primitive;

kii_json_util_parse_result_t kii_json_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    kii_json_primitive* result);

#ifdef __cplusplus
}
#endif
#endif
