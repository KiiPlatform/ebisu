#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_device.h"


extern kii_data_struct g_kii_data;

/*****************************************************************************
*
*  kiiDev_getToken
*
*  \param  vendorDeviceID - the input of identification of the device
*               password - the input of password
*
*  \return 0:success; -1: failure
*
*  \brief  Gets token
*
*****************************************************************************/
int kiiDev_getToken(char *vendorDeviceID, char *password)
{
    char * p1;
    char * p2;
    char *buf;
    char jsonBuf[256];

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
    strcpy(buf+strlen(buf), "/oauth2/token");
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
   strcpy(buf+strlen(buf), "application/vnd.kii.OauthTokenRequest+json");
   strcpy(buf+strlen(buf), STR_CRLF);

   memset(jsonBuf, 0, sizeof(jsonBuf));
   strcpy(jsonBuf, "{\"username\":\"VENDOR_THING_ID:");
   strcpy(jsonBuf+strlen(jsonBuf), vendorDeviceID);
   strcpy(jsonBuf+strlen(jsonBuf), "\",\"password\":\"");
   strcpy(jsonBuf+strlen(jsonBuf), password);
   strcpy(jsonBuf+strlen(jsonBuf), "\",\"grant_type\": \"password\"}");
   
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonBuf)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonBuf)+1) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	kiiHal_free(buf);
        return -1;
    }
   strcpy(buf+strlen(buf), jsonBuf);
   strcpy(buf+strlen(buf), STR_LF);
   
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
	kiiHal_free(buf);
        return -1;
    }

    if (strstr(buf, "HTTP/1.1 200") == NULL)
    {
	kiiHal_free(buf);
        return -1;    
    }

    //get access token
    p1 = strstr(buf, "\"access_token\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(g_kii_data.accessToken, 0, KII_ACCESS_TOKEN_SIZE+1);
    memcpy(g_kii_data.accessToken, p1, p2-p1);

    memset(g_kii_data.vendorDeviceID, 0, KII_DEVICE_VENDOR_ID+1);
    strcpy(g_kii_data.vendorDeviceID, vendorDeviceID);
    kiiHal_free(buf);
    return 0;
}


/*****************************************************************************
*
*  kiiDev_register
*
*  \param  vendorDeviceID - the input of identification of the device
*               deviceType - the input of device type
*               password - the input of password
*
*  \return 0:success; -1: failure
*
*  \brief  Registers device
*
*****************************************************************************/
int kiiDev_register(char *vendorDeviceID, char *deviceType, char *password)
{
    char * p1;
    char * p2;
    char *buf;
    char jsonBuf[256];

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
    strcpy(buf+strlen(buf), "/things/");
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
   strcpy(buf+strlen(buf), "application/vnd.kii.ThingRegistrationAndAuthorizationRequest+json");
   strcpy(buf+strlen(buf), STR_CRLF);

   memset(jsonBuf, 0, sizeof(jsonBuf));
   strcpy(jsonBuf, "{\"_vendorThingID\": \"");
   strcpy(jsonBuf+strlen(jsonBuf), vendorDeviceID);
   strcpy(jsonBuf+strlen(jsonBuf), "\",\"_thingType\": \"");
   strcpy(jsonBuf+strlen(jsonBuf), deviceType);
   strcpy(jsonBuf+strlen(jsonBuf), "\",\"_password\":\"");
   strcpy(jsonBuf+strlen(jsonBuf), password);
   strcpy(jsonBuf+strlen(jsonBuf), "\"}");
   
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonBuf)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonBuf)+1) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
        kiiHal_free(buf);
        return -1;
    }
   strcpy(buf+strlen(buf), jsonBuf);
   strcpy(buf+strlen(buf), STR_LF);
   
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if (strstr(buf, "HTTP/1.1 201") == NULL)
    {
        kiiHal_free(buf);
        return -1;    
    }

    p1 = strstr(buf, "\"_accessToken\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(g_kii_data.accessToken, 0, KII_ACCESS_TOKEN_SIZE+1);
    memcpy(g_kii_data.accessToken, p1, p2-p1);

    memset(g_kii_data.vendorDeviceID, 0, KII_DEVICE_VENDOR_ID+1);
    strcpy(g_kii_data.vendorDeviceID, vendorDeviceID);

    kiiHal_free(buf);
    return 0;
}


