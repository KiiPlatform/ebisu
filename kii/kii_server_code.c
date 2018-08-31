#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"

int kii_server_code_execute(kii_t* kii, const char* endpoint_name, const char* params)
{
    int ret = -1;

    char resource_path[256];
    size_t params_size = 0;

    memset(resource_path, 0x00, sizeof(resource_path));
    strcpy(resource_path, "api/apps/");
    strcat(resource_path, kii->_app_id);
    strcat(resource_path, "/server-code/versions/current/");
    strcat(resource_path, endpoint_name);
    
    if(params != NULL) {
        params_size = strlen(params);
    }
    int api_ret = kii_api_call(
            kii,
            "POST",
            resource_path,
            params,
            params_size,
            "application/json",
            NULL);
    if (api_ret != 0) {
        goto exit;
    }

    int resp_code = khc_get_status_code(&kii->_khc);
    if(resp_code < 200 || 300 <= resp_code) {
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=dos: */

