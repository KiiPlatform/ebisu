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
        return ret;
    }

    ret = _onboard(kii, out_token.token, vendor_thing_id, password, thing_type, firmware_version, layout_position, thing_properties);
    if (ret != KII_ERR_OK) {
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
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* opt_content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    const char* content_type = opt_content_type;
    if (opt_content_type == NULL) {
        content_type = "application/vnd.kii.MultipleTraitState+json";
    }
    return _upload_state(kii, state_read_cb, state_read_cb_data, "states", "PUT", content_type, opt_content_encoding, opt_normalizer_host);
}

kii_code_t kii_ti_put_bulk_states(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* opt_content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    const char* content_type = opt_content_type;
    if (opt_content_type == NULL) {
        content_type = "application/vnd.kii.StateBulkUploadRequest+json";
    }
    return _upload_state(kii, state_read_cb, state_read_cb_data, "states-bulk", "POST", content_type, opt_content_encoding, opt_normalizer_host);
}

kii_code_t kii_ti_patch_state(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* opt_content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    const char* content_type = opt_content_type;
    if (opt_content_type == NULL) {
        content_type = "application/vnd.kii.MultipleTraitStatePatch+json";
    }
    return _upload_state(kii, state_read_cb, state_read_cb_data, "states", "PATCH", content_type, opt_content_encoding, opt_normalizer_host);
}

kii_code_t kii_ti_patch_bulk_states(
        kii_t* kii,
        KII_CB_READ state_read_cb,
        void* state_read_cb_data,
        const char* opt_content_type,
        const char* opt_content_encoding,
        const char* opt_normalizer_host)
{
    const char* content_type = opt_content_type;
    if (opt_content_type == NULL) {
        content_type = "application/vnd.kii.StatePatchBulkUploadRequest+json";
    }
    return _upload_state(kii, state_read_cb, state_read_cb_data, "states-bulk", "POST", content_type, opt_content_encoding, opt_normalizer_host);
}
