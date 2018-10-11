#ifndef _kii_json_utils_
#define _kii_json_utils_

#ifdef __cplusplus
extern "C" {
#endif

#include "kii_json.h"

typedef enum kii_json_parse_primitive_result_t
{
    KII_JSON_PARSE_PRIMITIVE_RESULT_SUCCESS,
    KII_JSON_PARSE_PRIMITIVE_RESULT_OVERFLOW,
    KII_JSON_PARSE_PRIMITIVE_RESULT_UNDERFLOW,
    KII_JSON_PARSE_PRIMITIVE_RESULT_INVALID
} kii_json_parse_primitive_result_t;

typedef struct kii_json_primitive_t {
    union {
        int int_value;
        long long_value;
        double double_value;
        kii_json_boolean_t boolean_value;
    } value;
    kii_json_field_type_t type;
} kii_json_primitive_t;

kii_json_parse_primitive_result_t kii_json_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    kii_json_primitive_t* result);

#ifdef __cplusplus
}
#endif
#endif
