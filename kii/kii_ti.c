#include "kii.h"
#include "kii_ti_impl.h"

kii_code_t kii_ti_onboard(
    kii_t* kii,
    const char* vendor_thing_id,
    const char* password,
    const char* thing_type,
    const char* firmware_version,
    const char* layout_position,
    const char* thing_properties)
{
    char token[128];
    _kii_anonymous_token_t out_token;
    kii_code_t ret = KII_ERR_FAIL;

    out_token.buf = token;
    out_token.buf_size = sizeof(token)/sizeof(token[0]);
    ret = _get_anonymous_token(kii, &out_token);
    if (ret != KII_ERR_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get anonymous token.\n"));
        return ret;
    }

    ret = _onboard(kii, token, vendor_thing_id, password, thing_type, firmware_version, layout_position, thing_properties);
    if (ret != KII_ERR_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to onboard.\n"));
        return ret;
    }

    return KII_ERR_OK;
}

kii_code_t kii_ti_put_firmware_version(
    kii_t* kii,
    const char* firmware_version)
{
    // TODO: implement it.
    return KII_ERR_FAIL;
}

kii_code_t kii_ti_get_firmware_version(
    kii_t* kii,
    kii_ti_firmware_version_t* version)
{
    // TODO: implement it.
    return KII_ERR_FAIL;
}

kii_code_t kii_ti_put_state(
    kii_t* kii,
    size_t content_length,
    KII_CB_READ state_read_cb,
    kii_bool_t send_to_normalizer)
{
    // TODO: implement it.
    return KII_ERR_FAIL;
}

void kii_ti_set_normalizer_site(
    kii_t* kii,
    const char* normalizer_site)
{
    // TODO: implement it.
}
