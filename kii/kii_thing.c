#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_thing_impl.h"
#include "jkii_wrapper.h"

kii_code_t kii_auth_thing(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);
    kii_code_t ret = KII_ERR_FAIL;

    ret = _thing_auth(kii, vendor_thing_id, password);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        ret = KII_ERR_FAIL;
        goto exit;
    }
    ret = _kii_read_author(kii, buff, buff_size, "id", "access_token");
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    ret = KII_ERR_OK;

exit:
    return ret;
}

kii_code_t kii_register_thing(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    _reset_buff(kii);
    khc_reset_except_cb(&kii->_khc);
    kii_code_t ret = KII_ERR_FAIL;

    ret = _thing_register(kii, vendor_thing_id,
            password, thing_type);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    /* parse response */
    char* buff = kii->_rw_buff;
    size_t buff_size = kii->_rw_buff_written;
    if (buff == NULL) {
        ret = KII_ERR_FAIL;
        goto exit;
    }

    ret = _kii_read_author(kii, buff, buff_size, "_thingID", "_accessToken");
    if (ret != KII_ERR_OK) {
        goto exit;
    }
    ret = KII_ERR_OK;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
