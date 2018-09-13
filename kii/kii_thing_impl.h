#ifndef __kii_thing_impl__
#define __kii_thing_impl__

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

#endif