#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii-core/kii.h"
#include "kii_core_impl.h"
#include "kii_socket_impl.h"
#include "kii_task_impl.h"

int kii_init(kii_t* kii, char* site, char* appID, char* appKey)
{
	memset(kii, 0, sizeof(kii_t));
	if((strlen(site) != KII_SITE_SIZE) || (strlen(appID) != KII_APPID_SIZE) || (strlen(appKey) != KII_APPKEY_SIZE))
	{
		return -1;
	}
	else if(strcmp(site, "CN") == 0)
	{
		kii->app_host = "api-cn2.kii.com";
	}
	else if(strcmp(site, "JP") == 0)
	{
		kii->app_host = "api-jp.kii.com";
	}
	else if(strcmp(site, "US") == 0)
	{
		kii->app_host = "api.kii.com";
	}
	else if(strcmp(site, "SG") == 0)
	{
		kii->app_host = "api-sg.kii.com";
	}
	else
	{
        /* Let's enable to set custom host */
		kii->app_host = site;
	}

	kii->app_id = appID;
	kii->app_key = appKey;

    /* setting http callbacks. */
    kii->http_set_request_line_cb = request_line_cb;
    kii->http_set_header_cb = header_cb;
    kii->http_set_body_cb = body_cb;
    kii->http_execute_cb = execute_cb;

    /* setting logger callbacks. */
    kii->logger_cb = logger_cb;

    /* setting socket callbacks. */
    kii->socket_connect_cb = connect_cb;
    kii->socket_send_cb = send_cb;
    kii->socket_recv_cb = recv_cb;
    kii->socket_close_cb = close_cb;

    /* setting task callbacks. */
    kii->task_create_cb = task_create_cb;
    kii->delay_ms_cb = delay_ms_cb;

	return 0;
}
