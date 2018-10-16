#ifndef _jkii_utils_
#define _jkii_utils_

#ifdef __cplusplus
extern "C" {
#endif

#include "jkii.h"

typedef enum jkii_parse_primitive_result_t
{
    JKII_PARSE_PRIMITIVE_RESULT_SUCCESS,
    JKII_PARSE_PRIMITIVE_RESULT_OVERFLOW,
    JKII_PARSE_PRIMITIVE_RESULT_UNDERFLOW,
    JKII_PARSE_PRIMITIVE_RESULT_INVALID
} jkii_parse_primitive_result_t;

typedef struct jkii_primitive_t {
    union {
        int int_value;
        long long_value;
        double double_value;
        jkii_boolean_t boolean_value;
    } value;
    jkii_field_type_t type;
} jkii_primitive_t;

jkii_parse_primitive_result_t jkii_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    jkii_primitive_t* result);

int jkii_escape_str(const char* str, char* buff, size_t buff_size);

#ifdef __cplusplus
}
#endif
#endif
