#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"

#include "kii_core.h"

int kii_server_code_execute(kii_t* kii, const char* endpoint_name, const char* params)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;
    char resource_path[256];
    size_t params_size = 0;

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->kii_core.app_id);
    strcat(resource_path, "/server-code/versions/current/");
    strcat(resource_path, endpoint_name);
    
    if(params != NULL) {
        params_size = strlen(params);
    }
    core_err = kii_core_api_call(
            &kii->kii_core,
            "POST",
            resource_path,
            params,
            params_size,
            "application/json",
            NULL);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_core_run(&kii->kii_core);
        state = kii_core_get_state(&kii->kii_core);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->kii_core.logger_cb("resp: %s\n", kii->kii_core.response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->kii_core.response_code < 200 || 300 <= kii->kii_core.response_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=dos: */

