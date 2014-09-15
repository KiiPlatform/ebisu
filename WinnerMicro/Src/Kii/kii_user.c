#include <string.h>
#include <stdio.h>

#include "kii_def.h"
#include "kii_user.h"
#include "kii_hal.h"

extern kii_data_struct g_kii_data;


int kiiUser_logIn(char *userName, char *password)
{
    char * p1;
    char * p2;
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/oauth2/token");
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
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(userName)+strlen(password)+30);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), "{\"username\":\"");
   strcpy(buf+strlen(buf), userName);
   strcpy(buf+strlen(buf), "\", \"password\":\"");
   strcpy(buf+strlen(buf), password);
   strcpy(buf+strlen(buf), "\"}\n");

    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;

    p1 = strstr(buf, "access_token");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    if (p1 == NULL)
    {
        KII_DEBUG("kii-error: log in failed !\r\n");
        return -1;
    }
    p1 +=1;
    p2 = strstr(p1, "\"");
    if (p2 == NULL)
    {
        KII_DEBUG("kii-error: log in failed !\r\n");
        return -1;
    }
    memset(g_kii_data.accessToken, 0, KII_ACCESS_TOKEN_SIZE+1);
    memcpy(g_kii_data.accessToken, p1, p2-p1);	
    KII_DEBUG("kii-info: accessToken:\"%s\"\r\n", g_kii_data.accessToken);
    return 0;
}


