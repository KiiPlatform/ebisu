#ifndef KII_THING_IMPL_H
#define KII_THING_IMPL_H

#include "kii.h"

kii_code_t _thing_auth(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password);

kii_code_t _thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type);

kii_code_t _thing_refresh(
        kii_t* kii,
        const char* refresh_token);

#endif
