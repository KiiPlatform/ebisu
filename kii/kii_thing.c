#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_json_utils.h"

#define KII_SDK_INFO "sn=te;sv=1.2.4"

int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key)
{
    memset(kii, 0x00, sizeof(kii_t));
    kii->_app_id = app_id;
    kii->_app_key = app_key;
        if(strcmp(site, "CN") == 0)
    {
        kii->_app_host = "api-cn2.kii.com";
    }
    else if(strcmp(site, "CN3") == 0)
    {
        kii->_app_host = "api-cn3.kii.com";
    }
    else if(strcmp(site, "JP") == 0)
    {
        kii->_app_host = "api-jp.kii.com";
    }
    else if(strcmp(site, "US") == 0)
    {
        kii->_app_host = "api.kii.com";
    }
    else if(strcmp(site, "SG") == 0)
    {
        kii->_app_host = "api-sg.kii.com";
    }
    else if (strcmp(site, "EU") == 0)
    {
        kii->_app_host = "api-eu.kii.com";
    }
    else
    {
        /* Let's enable to set custom host */
        kii->_app_host = (char*)site;
    }
    kii->_sdk_info = KII_SDK_INFO;
    return 0;
}

static khc_code _thing_authentication(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password
        )
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

static khc_code _register_thing_with_id(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password,
        const char* thing_type)
{
    // TODO: reimplement it.
    return KHC_ERR_FAIL;
}

int kii_thing_authenticate(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    int ret = -1;

    khc_code khc_err = _thing_authentication(kii, vendor_thing_id, password);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    // TODO: get response code.
    int respCode;
    if(respCode < 200 || 300 <= respCode) {
        goto exit;
    }

    // TODO: get buffer and its length.
    char* buff;
    size_t buff_size;
    if (buff == NULL) {
        goto exit;
    }
    kii_json_field_t fields[3];
    kii_json_parse_result_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "id";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.author_id;
    fields[0].field_copy_buff_size = sizeof(kii->_author.author_id) /
            sizeof(kii->_author.author_id[0]);
    fields[1].name = "access_token";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.access_token;
    fields[1].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }

    ret = 0;

exit:
    return ret;
}

int kii_thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    int ret = -1;

    khc_code khc_err = _register_thing_with_id(kii, vendor_thing_id,
            password, thing_type);
    if (khc_err != KHC_ERR_OK) {
        goto exit;
    }

    // TODO: get response code.
    int respCode;
    if(respCode < 200 || 300 <= respCode) {
        goto exit;
    }

    /* parse response */
    // TODO: get buffer and its length.
    char* buff;
    size_t buff_size;
    if (buff == NULL) {
        goto exit;
    }

    kii_json_field_t fields[3];
    kii_json_parse_result_t result;
    memset(fields, 0, sizeof(fields));
    fields[0].name = "_accessToken";
    fields[0].type = KII_JSON_FIELD_TYPE_STRING;
    fields[0].field_copy.string = kii->_author.access_token;
    fields[0].field_copy_buff_size = sizeof(kii->_author.access_token) /
            sizeof(kii->_author.access_token[0]);
    fields[1].name = "_thingID";
    fields[1].type = KII_JSON_FIELD_TYPE_STRING;
    fields[1].field_copy.string = kii->_author.author_id;
    fields[1].field_copy_buff_size = sizeof(kii->_author.author_id) /
            sizeof(kii->_author.author_id[0]);
    fields[2].name = NULL;

    result = prv_kii_json_read_object(kii, buff, buff_size, fields);
    if (result != KII_JSON_PARSE_SUCCESS) {
        ret = -1;
        goto exit;
    }
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
