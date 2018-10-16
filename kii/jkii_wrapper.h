#ifndef JKII_UTILS_H
#define JKII_UTILS_H

#include "kii.h"

#ifdef __cplusplus
extern "C" {
#endif

jkii_parse_err_t _jkii_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        jkii_field_t *fields);

#ifdef __cplusplus
}
#endif

#endif
