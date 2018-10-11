#include "kii.h"
#include "kii_json_wrapper.h"

#include <string.h>

kii_json_parse_result_t _kii_json_read_object(
        kii_t* kii,
        const char* json_string,
        size_t json_string_size,
        kii_json_field_t *fields)
{
    kii_json_parse_result_t res = KII_JSON_PARSE_INVALID_INPUT;
    kii_json_resource_t* resource = kii->_json_resource;
    if (resource == NULL) {
        res = kii_json_parse_with_allocator(json_string, json_string_size, fields, kii->_json_alloc_cb, kii->_json_free_cb);
    } else {
        res = kii_json_parse(json_string, json_string_size, fields, kii->_json_resource);
    }

    return res;
}
