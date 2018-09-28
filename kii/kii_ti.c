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
    _kii_token_t out_token;
    kii_code_t ret = KII_ERR_FAIL;

    ret = _get_anonymous_token(kii, &out_token);
    if (ret != KII_ERR_OK) {
        M_KII_LOG(kii->kii_core.logger_cb("fail to get anonymous token.\n"));
        return ret;
    }

    ret = _onboard(kii, out_token.token, vendor_thing_id, password, thing_type, firmware_version, layout_position, thing_properties);
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
    return _put_firmware_version(kii, firmware_version);
}

kii_code_t kii_ti_get_firmware_version(
    kii_t* kii,
    kii_ti_firmware_version_t* version)
{
    return _get_firmware_version(kii, version);
}

kii_code_t kii_ti_put_state(
    kii_t* kii,
    size_t content_length,
    KII_CB_READ state_read_cb,
    void* userdata,
    const char* opt_normalizer_host)
{
    return _put_state(kii, content_length, state_read_cb, userdata, opt_normalizer_host);
}
