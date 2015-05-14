#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"

#include "kii-core/kii_core.h"
#include "kii_core_impl.h"

int kii_thing_authenticate(
        kii_core_t* kii,
        const char* vendor_thing_id,
        const char* password)
{
    char* p1;
    char* p2;
    char* buf;
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->http_context.buffer;
    core_err = kii_core_thing_authentication(kii, vendor_thing_id, password);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(kii);
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code) {
        goto exit;
    }
    p1 = strstr(buf, "\"access_token\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL) {
        goto exit;
    }
    strcpy(kii->author.author_id, vendor_thing_id);
    memcpy(kii->author.access_token, p1, p2 - p1);
    ret = 0;

exit:
    return ret;
}

int kii_thing_register(
        kii_core_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password)
{
    char* buf;
    char* p1;
    char* p2;
    char thing_data[1024];
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->http_context.buffer;
    sprintf(thing_data,
            "{\"_vendorThingID\":\"%s\",\"_thingType\":\"%s\",\"_password\":\"%s\"}",
            vendor_thing_id, thing_type, password);
    core_err = kii_core_register_thing(kii, thing_data);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(kii); 
        state = kii_core_get_state(kii);
    } while (state != KII_STATE_IDLE);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code) {
        goto exit;
    }
    /* parse response */
    p1 = strstr(buf, "\"_accessToken\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, ":");
    if(p1 == NULL) {
        goto exit;
    }
    p1 = strstr(p1, "\"");
    if(p1 == NULL) {
        goto exit;
    }
    p1 += 1;
    p2 = strstr(p1, "\"");
    if(p2 == NULL) {
        goto exit;
    }
    memcpy(kii->author.access_token, p1, p2 - p1);
    strcpy(kii->author.author_id, vendor_thing_id);
    ret = 0;

exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=dos: */
