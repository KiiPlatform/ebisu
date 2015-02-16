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
int kiiExt_extension(char *endpointName, char *jsonObject)
{
    char *buf;

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/server-code/versions/current/");
    strcpy(buf+strlen(buf), endpointName);
    strcpy(buf+strlen(buf), STR_HTTP);
   strcpy(buf+strlen(buf), STR_CRLF);
   //Connection
   strcpy(buf+strlen(buf), "Connection: Keep-Alive\r\n");
   //Host
   strcpy(buf+strlen(buf), "Host: ");
   strcpy(buf+strlen(buf), g_kii_data.host);
   strcpy(buf+strlen(buf), STR_CRLF);
    //x-kii-appid
    strcpy(buf+strlen(buf), STR_KII_APPID);
    strcpy(buf+strlen(buf), g_kii_data.appID); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //x-kii-appkey 
    strcpy(buf+strlen(buf), STR_KII_APPKEY);
    strcpy(buf+strlen(buf), g_kii_data.appKey);
   strcpy(buf+strlen(buf), STR_CRLF);
   //content-type	
   strcpy(buf+strlen(buf), STR_CONTENT_TYPE);
   strcpy(buf+strlen(buf), "application/json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonObject)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonObject)+1) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	kiiHal_free(buf);
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);
   strcpy(buf+strlen(buf), STR_LF);
   
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if ((strstr(buf, "HTTP/1.1 200") == NULL))
    {
	kiiHal_free(buf);
        return -1;    
    }
    else
    {
        kiiHal_free(buf);
        return 0;
    }
}

