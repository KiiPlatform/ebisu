#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_extension.h"
#include "kii_hal.h"

extern kii_data_struct g_kii_data;

/*****************************************************************************
*
*  kiiExt_extension
*
*  \param  endpointName - the endpoint name
*              jsonObject - the input of object with json format
*
*  \return 0:success; -1: failure
*
*  \brief  Executes the server extension code
*
*****************************************************************************/
int kiiExt_extension(char* endpointName, char* jsonObject)
{
	char* buf;
	int ret = -1;
	
	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_POST);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	strcat(buf, "/server-code/versions/current/");
	strcat(buf, endpointName);
	strcat(buf, STR_HTTP);
	strcat(buf, STR_CRLF);
	// Host
	strcat(buf, "Host: ");
	strcat(buf, g_kii_data.host);
	strcat(buf, STR_CRLF);
	// x-kii-appid
	strcat(buf, STR_KII_APPID);
	strcat(buf, g_kii_data.appID);
	strcat(buf, STR_CRLF);
	// x-kii-appkey
	strcat(buf, STR_KII_APPKEY);
	strcat(buf, g_kii_data.appKey);
	strcat(buf, STR_CRLF);
	// content-type
	strcat(buf, STR_CONTENT_TYPE);
	strcat(buf, "application/json");
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", strlen(jsonObject) + 1);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + strlen(jsonObject) + 1) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		goto free;
	}
	strcat(buf, jsonObject);
	strcat(buf, STR_LF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		goto free;
	}

	if((strstr(buf, "HTTP/1.1 200") != NULL))
	{
		ret = 0;
	}
	
free:
	kiiHal_free(buf);
exit:
	return ret;
}
