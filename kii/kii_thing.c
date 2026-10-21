#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_impl.h"
#include "kii_thing_impl.h"
#include "kii_session_impl.h"
#include "jkii_wrapper.h"

kii_code_t kii_auth_thing(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    _reset_buff(kii);
    _reset_khc(kii);
    kii_code_t ret = KII_ERR_FAIL;

    ret = _thing_auth(kii, vendor_thing_id, password);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        ret = KII_ERR_RESP_STATUS;
        goto exit;
    }

    // A login starts a new pair, which has no refresh token unless the server
    // issues one.
    kii->_author.refresh_token[0] = '\0';
    int64_t expires_in_sec = 0;
    ret = _kii_parse_token_response(kii, &kii->_author, KII_TRUE, &expires_in_sec);
    if (ret != KII_ERR_OK) {
        goto exit;
    }
    if (kii->_session != NULL) {
        _kii_session_publish(kii, expires_in_sec, KII_FALSE);
    }

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
    _reset_khc(kii);
    kii_code_t ret = KII_ERR_FAIL;

    ret = _thing_register(kii, vendor_thing_id,
            password, thing_type);
    if (ret != KII_ERR_OK) {
        goto exit;
    }

    uint16_t resp_code = khc_get_status_code(&kii->_khc);
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

    kii->_author.refresh_token[0] = '\0';
    jkii_field_t fields[3];
    jkii_parse_err_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "_accessToken";
    fields[0].type = JKII_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
        sizeof(kii->_author.access_token[0]);
    fields[1].name = "_thingID";
    fields[1].type = JKII_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->_author.author_id) /
        sizeof(kii->_author.author_id[0]);
    fields[2].name = NULL;

    result = _jkii_read_object(kii, buff, buff_size, fields);
    if (result != JKII_ERR_OK) {
        ret = KII_ERR_PARSE_JSON;
        goto exit;
    }
    ret = KII_ERR_OK;
    if (kii->_session != NULL) {
        _kii_session_publish(kii, 0, KII_FALSE);
    }

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
