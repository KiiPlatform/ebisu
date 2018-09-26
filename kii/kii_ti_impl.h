#ifndef __kii_ti_impl__
#define __kii_ti_impl__

#include "kii.h"

typedef struct {
    char* buf;
    size_t buf_size;
} _kii_anonymous_token_t;

kii_code_t _get_anonymous_token(
        kii_t* kii,
        _kii_anonymous_token_t* out_token);

kii_code_t _onboard(
        kii_t* kii,
        const char* token,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties);

#endif
