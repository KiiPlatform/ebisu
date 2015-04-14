#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii-core/kii.h"
#include "kii_core_impl.h"

kii_t g_kii_data;
context_t ctx;

/*****************************************************************************
*
*  kii_init
*
*  \param  site - the input of site name, should be one of "CN", "JP", "US", "SG"
*              appID - the input of Application ID
*              objectID - the input of Application Key
*
*  \return  0:success; -1: failure
*
*  \brief  Initializes Kii
*
*****************************************************************************/
int kii_init(char* site, char* appID, char* appKey)
{
    /* TODO: logging improvement */
	memset(&g_kii_data, 0, sizeof(kii_t));
	if((strlen(site) != KII_SITE_SIZE) || (strlen(appID) != KII_APPID_SIZE) || (strlen(appKey) != KII_APPKEY_SIZE))
	{
		return -1;
	}
	else if(strcmp(site, "CN") == 0)
	{
		g_kii_data.app_host = "api-cn2.kii.com";
	}
	else if(strcmp(site, "JP") == 0)
	{
		g_kii_data.app_host = "api-jp.kii.com";
	}
	else if(strcmp(site, "US") == 0)
	{
		g_kii_data.app_host = "api.kii.com";
	}
	else if(strcmp(site, "SG") == 0)
	{
		g_kii_data.app_host = "api-sg.kii.com";
	}
	else
	{
        /* Let's enable to set custom host */
		g_kii_data.app_host = site;
	}

	g_kii_data.app_id = appID;
	g_kii_data.app_key = appKey;

    /* setting callbacks. */
    g_kii_data.http_set_request_line_cb = request_line_cb;
    g_kii_data.http_set_header_cb = header_cb;
    g_kii_data.http_set_body_cb = body_cb;
    g_kii_data.http_execute_cb = execute_cb;
    g_kii_data.logger_cb = logger_cb;

    memset(&ctx, 0, sizeof(context_t));
    g_kii_data.http_context = &ctx;

	return 0;
}
