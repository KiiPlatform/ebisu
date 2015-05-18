#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_core.h"
#include "kii_core_impl.h"
#include "kii_socket_impl.h"
#include "kii_secure_socketl_impl.h"
#include "kii_task_impl.h"

int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key)
{
    memset(kii, 0, sizeof(kii_t));
    if(strcmp(site, "CN") == 0)
    {
        kii->kii_core.app_host = "api-cn2.kii.com";
    }
    else if(strcmp(site, "JP") == 0)
    {
        kii->kii_core.app_host = "api-jp.kii.com";
    }
    else if(strcmp(site, "US") == 0)
    {
        kii->kii_core.app_host = "api.kii.com";
    }
    else if(strcmp(site, "SG") == 0)
    {
        kii->kii_core.app_host = "api-sg.kii.com";
    }
    else
    {
        /* Let's enable to set custom host */
        kii->kii_core.app_host = (char*)site;
    }

    kii->kii_core.app_id = (char*)app_id;
    kii->kii_core.app_key = (char*)app_key;

    /* setting http callbacks. */
    kii->kii_core.http_set_request_line_cb = request_line_cb;
    kii->kii_core.http_set_header_cb = header_cb;
    kii->kii_core.http_set_body_cb = body_cb;
    kii->kii_core.http_execute_cb = execute_cb;

    /* setting http socket callbacks */
    kii->kii_core.http_context.connect_cb = s_connect_cb;
    kii->kii_core.http_context.send_cb = s_send_cb;
    kii->kii_core.http_context.recv_cb = s_recv_cb;
    kii->kii_core.http_context.close_cb = s_close_cb;

    /* setting logger callbacks. */
    kii->kii_core.logger_cb = logger_cb;

    /* setting mqtt socket callbacks. */
    kii->mqtt_socket_connect_cb = connect_cb;
    kii->mqtt_socket_send_cb = send_cb;
    kii->mqtt_socket_recv_cb = recv_cb;
    kii->mqtt_socket_close_cb = close_cb;

    /* setting task callbacks. */
    kii->task_create_cb = task_create_cb;
    kii->delay_ms_cb = delay_ms_cb;

    return 0;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */

