#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"

kii_code_t kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        kii_bool_t set_authentication_header);

kii_code_t kii_api_call_set_no_body(
        kii_t* kii);

kii_code_t kii_api_call_append_body(
        kii_t* kii,
        const char* chunk,
        size_t chunk_size);

kii_code_t kii_api_call_append_header_or_default(
        kii_t* kii,
        const char* key,
        const char* value,
        const char* default_value);

kii_code_t kii_api_call_append_header(
        kii_t* kii,
        const char* key,
        const char* value);

kii_code_t kii_api_call_run(kii_t* kii);

#endif