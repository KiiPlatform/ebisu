#ifndef __kii_ti_impl__
#define __kii_ti_impl__

#include "kii.h"

kii_code_t _onboard(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type,
        const char* firmware_version,
        const char* layout_position,
        const char* thing_properties);

kii_code_t _put_firmware_version(
        kii_t* kii,
        const char* firmware_version);

kii_code_t _get_firmware_version(
        kii_t* kii,
        kii_ti_firmware_version_t* out_version);

kii_code_t _upload_state(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* last_path_segment,
        const char* method,
        const char* content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host);

#endif
