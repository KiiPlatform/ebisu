#ifndef __kii_ti_impl__
#define __kii_ti_impl__

#include "kii.h"

kii_code_t _get_anonymous_token(
        kii_t* kii,
        char* token_buf,
        size_t token_buf_size);

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
