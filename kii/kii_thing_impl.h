#ifndef __kii_thing_impl__
#define __kii_thing_impl__

#include "kii.h"

kii_code_t _thing_authentication(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password);

kii_code_t _register_thing_with_id(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type);

#endif