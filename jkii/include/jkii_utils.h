#ifndef _jkii_utils_
#define _jkii_utils_

#ifdef __cplusplus
extern "C" {
#endif

#include "jkii.h"

typedef enum jkii_primitive_err_t
{
    JKII_PRIMITIVE_ERR_OK,
    JKII_PRIMITIVE_ERR_OVERFLOW,
    JKII_PRIMITIVE_ERR_UNDERFLOW,
    JKII_PRIMITIVE_ERR_INVALID
} jkii_primitive_err_t;

typedef struct jkii_primitive_t {
    union {
        int int_value;
        long long_value;
        double double_value;
        jkii_boolean_t boolean_value;
    } value;
    jkii_field_type_t type;
} jkii_primitive_t;

jkii_primitive_err_t jkii_parse_primitive(
    const char* primitive,
    size_t primitive_length,
    jkii_primitive_t* result);

int jkii_escape_str(const char* str, char* buff, size_t buff_size);

#ifdef __cplusplus
}
#endif
#endif
