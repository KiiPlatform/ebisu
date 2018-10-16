#include "kii.h"
#include "jkii_wrapper.h"

#include <string.h>

jkii_parse_result_t _jkii_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        jkii_field_t *fields)
{
    jkii_parse_result_t res = JKII_INVALID_INPUT;
    jkii_resource_t* resource = kii->_json_resource;
    if (resource == NULL) {
        res = jkii_parse_with_allocator(json_string, json_string_size, fields, kii->_json_alloc_cb, kii->_json_free_cb);
    } else {
        res = jkii_parse(json_string, json_string_size, fields, kii->_json_resource);
    }

    return res;
}
