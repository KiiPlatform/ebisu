#include <string.h>
#include <stdio.h>

#include "kii_def.h"
#include "kii_meta.h"
#include "kii_object.h"
#include "kii_hal.h"
extern char g_netBuf[KII_NETBUF_SIZE];
extern kii_meta_struct g_kiiMeta;


extern char *kii_getHost(void);
extern char *kii_getAppID(void);
extern char * kii_getAppKey(void);


/*****************************************************************************
*
*  kiiObj_create
*
*  \param: bucketName: the input of bucket name
*               jsonObject: the input of object with json format
*               objectID: the output of objectID
*
*  \return 0:success, -1: failure
*
*  \brief  create object
*
*****************************************************************************/
int kiiObj_create(char *bucketName, char *jsonObject, char *objectID)
{
    int socketNum;
    char * p1;
    char * p2;
    char *buf;
    unsigned char ipBuf[4];

    buf = g_netBuf;
	
    memset(buf, 0, KII_NETBUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "http://");
    strcpy(buf+strlen(buf), kii_getHost());
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), kii_getAppID());
    strcpy(buf+strlen(buf), "/users/me/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects");
    strcpy(buf+strlen(buf), STR_HTTP);
   strcpy(buf+strlen(buf), STR_CRLF);
   //Connection
   strcpy(buf+strlen(buf), "Connection: Keep-Alive\r\n");
   //Host
   strcpy(buf+strlen(buf), "Host: ");
   strcpy(buf+strlen(buf), kii_getHost());
   strcpy(buf+strlen(buf), STR_CRLF);
    //x-kii-appid
    strcpy(buf+strlen(buf), STR_KII_APPID);
    strcpy(buf+strlen(buf), kii_getAppID()); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //x-kii-appkey 
    strcpy(buf+strlen(buf), STR_KII_APPKEY);
    strcpy(buf+strlen(buf), kii_getAppKey());
   strcpy(buf+strlen(buf), STR_CRLF);
   //content-type	
    strcpy(buf+strlen(buf), STR_CONTENT_TYPE);
    strcpy(buf+strlen(buf), "application/vnd.");
    strcpy(buf+strlen(buf), kii_getAppID());
    strcpy(buf+strlen(buf), ".mydata+json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kiiMeta.accessToken); //the access token musb be checked before calling kiiObj_create
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d\r\n", strlen(jsonObject));
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonObject)) > KII_NETBUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow!\r\n");
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);

    if (kiiHAL_dns(kii_getHost(), ipBuf) < 0)
    {
        KII_DEBUG("kii-error: dns failed !\r\n");
        return -1;
    }
    KII_DEBUG("Host ip:%d.%d.%d.%d\r\n", ipBuf[3], ipBuf[2], ipBuf[1], ipBuf[0]);
		
    socketNum = kiiHAL_socketCreate();
    if (socketNum < 0)
    {
        KII_DEBUG("kii-error: create socket failed !\r\n");
        return -1;
    }
	
	
    if (kiiHAL_connect(socketNum, (char*)ipBuf) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
	 kiiHAL_socketClose(socketNum);
        return -1;
    }
    
    if (kiiHAL_socketSend(socketNum, buf, strlen(buf)) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHAL_socketClose(socketNum);
        return -1;
    }

    memset(buf, 0, KII_NETBUF_SIZE);
	
    if (kiiHAL_socketRecv(socketNum, buf, KII_NETBUF_SIZE) < 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHAL_socketClose(socketNum);
        return -1;
    }

    p1 = strstr(buf, "objectID");
    if (p1 == NULL)
    {
        KII_DEBUG("kii-error: get objectID fail\r\n");
	 kiiHAL_socketClose(socketNum);
        return -1;
    }
    p1 +=3;
    p2 = strstr(p1, ",");
    if (p2 == NULL)
    {
        KII_DEBUG("kii-error: get objectID fail\r\n");
	 kiiHAL_socketClose(socketNum);
        return -1;
    }
    memcpy(objectID, p1, p2-p1);
     kiiHAL_socketClose(socketNum);
    return 0;
}



