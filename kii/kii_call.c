#include "kii.h"

#include <kii_core.h>

int kii_api_call_start(
        kii_t* kii,
        const char* http_method,
        const char* resource_path,
        const char* content_type,
        kii_bool_t set_authentication_header)
{
    return kii_core_api_call_start(&(kii->kii_core), http_method, resource_path,
            content_type, set_authentication_header) == KIIE_OK ? 0 : -1;
}

int kii_api_call_append_body(
        kii_t* kii,
        const char* body_data,
        size_t body_size)
{
    return kii_core_api_call_append_body(&(kii->kii_core), body_data,
            body_size) == KIIE_OK ? 0 : -1;
}

int kii_api_call_append_header(kii_t* kii, const char* key, const char* value)
{
    return kii_core_api_call_append_header(&(kii->kii_core), key,
            value) == KIIE_OK ? 0 : -1;
}

int kii_api_call_run(kii_t* kii)
{
    if (kii_core_api_call_end(&(kii->kii_core)) != KIIE_OK) {
        return -1;
    } else {
        kii_error_code_t error = KIIE_OK;
        do {
            error = kii_core_run(&(kii->kii_core));
        } while (kii_core_get_state(&(kii->kii_core)) != KII_STATE_IDLE);
        return error == KIIE_OK ? 0 : -1;
    }
}
