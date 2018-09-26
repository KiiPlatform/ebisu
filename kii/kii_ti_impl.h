#ifndef __kii_ti_impl__
#define __kii_ti_impl__

#include "kii.h"

typedef struct {
    char token[64];
} _kii_token_t;

kii_code_t _get_anonymous_token(
        kii_t* kii,
        _kii_token_t* out_token);

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
