#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"

extern kii_data_struct g_kii_data;
static kii_push_struct m_kii_push;

#define    KIIPUSH_TASK_STK_SIZE      256
static unsigned int  mKiiPush_taskStk[KIIPUSH_TASK_STK_SIZE];     



/*****************************************************************************
*
*  kiiPush_install
*
*  \param: none
*
*  \return 0:success; -1: failure
*
*  \brief  register installation of a device
*
*****************************************************************************/
int kiiPush_install(void)
{
    char * p1;
    char * p2;
    char *buf;
    char jsonBuf[256];

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/installations");
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
   strcpy(buf+strlen(buf), "application/vnd.kii.InstallationCreationRequest+json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);

   memset(jsonBuf, 0, sizeof(jsonBuf));
    strcpy(jsonBuf, "{\"installationRegistrationID\":\"");
   strcpy(buf+strlen(buf),  g_kii_data.deviceID);
    strcpy(jsonBuf+strlen(buf), "\",\"deviceType\":\"IOT_MQTT\", \"development\":false}");

    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonBuf)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonBuf)+1) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
        return -1;
    }
   strcpy(buf+strlen(buf), jsonBuf);
   strcpy(buf+strlen(buf), STR_LF);
   
    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;

    p1 = strstr(buf, "HTTP/1.1 201");
    p1 = strstr(p1, "installationID");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
	
    if (p1 == NULL)
    {
	 return -1;
    }
    p1 +=1;
    p2 = strstr(p1, "\"");
    if (p2 == NULL)
    {
	 return -1;
    }
    memset(m_kii_push.installationID, 0, KII_PUSH_INSTALLATIONID_SIZE+1);
    memcpy(m_kii_push.installationID, p1, p2-p1);

    return 0;
}





/*****************************************************************************
*
*  kiiPush_retrieveEndpoint
*
*  \param: none
*
*  \return KIIPUSH_ENDPOINT_READY
*             KIIPUSH_ENDPOINT_UNAVAILABLE
*             KIIPUSH_ENDPOINT_ERROR
*
*  \brief  retrieve MQTT endpoint
*
*****************************************************************************/
kiiPush_endpointState_e kiiPush_retrieveEndpoint(void)
{
    char * p1;
    char *buf;
    char jsonBuf[256];

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_GET);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/installations/");
    strcpy(buf+strlen(buf), m_kii_push.installationID);
    strcpy(buf+strlen(buf), "/mqtt-endpoint");
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
   strcpy(buf+strlen(buf), "application/vnd.kii.InstallationCreationRequest+json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);

   memset(jsonBuf, 0, sizeof(jsonBuf));
    strcpy(jsonBuf, "{\"installationRegistrationID\":\"");
   strcpy(jsonBuf+strlen(jsonBuf),  g_kii_data.deviceID);
    strcpy(jsonBuf+strlen(jsonBuf), "\",\"deviceType\":\"IOT_MQTT\", \"development\":false}");

    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonBuf)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonBuf)+1) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
        return KIIPUSH_ENDPOINT_ERROR;
    }
   strcpy(buf+strlen(buf), jsonBuf);
   strcpy(buf+strlen(buf), STR_LF);
   
    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return KIIPUSH_ENDPOINT_ERROR;
    }
    buf = g_kii_data.rcvdBuf;

    p1 = strstr(buf, "HTTP/1.1 200");
	
    if (p1 != NULL)
    {
	 return KIIPUSH_ENDPOINT_READY;
    }
	
    p1 = strstr(buf, "HTTP/1.1 503");
	
    if (p1 != NULL)
    {
	 return KIIPUSH_ENDPOINT_UNAVAILABLE;
    }

    return KIIPUSH_ENDPOINT_ERROR;
}



/*****************************************************************************
*
*  kiiPush_subscribeBucket
*
*  \param: bucketID - the bucket ID
*
*  \return 0:success; -1: failure
*
*  \brief  subscribe bucket
*
*****************************************************************************/
int kiiPush_subscribeBucket(char *bucketID)
{
    char * p1;
    char * p2;
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketID);
    strcpy(buf+strlen(buf), "/filters/all/push/subscriptions/things");
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
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   
    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;

    p1 = strstr(buf, "HTTP/1.1 204");
    p2 = strstr(buf, "HTTP/1.1 409");
	
    if (p1 != NULL  || p2 != NULL)
    {
	 return 0;
    }
    else
    {
	return -1;
    }
}



/*****************************************************************************
*
*  kiiPush_subscribeTopic
*
*  \param: topicID - the topic ID
*
*  \return 0:success; -1: failure
*
*  \brief  subscribe topic
*
*****************************************************************************/
int kiiPush_subscribeTopic(char *topicID)
{
    char * p1;
    char * p2;
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/topics/");
    strcpy(buf+strlen(buf),topicID);
    strcpy(buf+strlen(buf), "/push/subscriptions/things");
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
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   
    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;

    p1 = strstr(buf, "HTTP/1.1 204");
    p2 = strstr(buf, "HTTP/1.1 409");
	
    if (p1 != NULL  || p2 != NULL)
    {
	 return 0;
    }
    else
    {
	return -1;
    }
}

static void kiiPush_task(void *sdata)
{
    kiiPush_recvMessageCallback callback;
    unsigned char netConnected = 0;
    int socketNum;
    unsigned char ipBuf[4];
    int rcvdCounter;

    callback = (kiiPush_recvMessageCallback) sdata;
    
	for(;;)
	{
	    if (netConnected == 0)
	    {
  	        kiiHal_delayMs(1000);
		if (kiiHal_getNetState() == 0)
		{
        		if (kiiHal_dns(m_kii_push.host, ipBuf) < 0)
        		{
        			KII_DEBUG("kii-error: push dns failed !\r\n");
        			continue;
        		}
        		KII_DEBUG("Push host ip:%d.%d.%d.%d\r\n", ipBuf[3], ipBuf[2], ipBuf[1], ipBuf[0]);
        			
        		socketNum = kiiHal_socketCreate();
        		if (socketNum < 0)
        		{
        			KII_DEBUG("kii-error: push create socket failed !\r\n");
        			continue;
        		}
        		if (kiiHal_connect(socketNum, (char*)ipBuf) < 0)
        		{
        			KII_DEBUG("kii-error: push connect to server failed \r\n");
        		        kiiHal_socketClose(socketNum);
        			continue;
        		}
			else 
			{
			    netConnected = 1;
			}
		}
		else
		{
		    kiiHal_delayMs(1000);
		    continue;
		}
	    }
	    else
	    {
			rcvdCounter = kiiHal_socketRecv(socketNum, m_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
			if (rcvdCounter < 0)
			{
				KII_DEBUG("kii-error: push recv data fail\r\n");
			        kiiHal_socketClose(socketNum);
				netConnected = 0 ;
			}
			else
			{
			 callback(m_kii_push.rcvdBuf);
		        }
	    }
	}
}


/*****************************************************************************
*
*  KiiPush_init
*
*  \param: taskPrio - the priority of task
*               callback - the call back function for processing the push message received
*
*  \return 0:success; -1: failure
*
*  \brief  init push
*
*****************************************************************************/
int KiiPush_init(unsigned int taskPrio, kiiPush_recvMessageCallback callback)
{
	kiiPush_endpointState_e endpointState;

    memset(&m_kii_push, 0, sizeof(m_kii_push));

        if (kiiPush_install() != 0)
        {
            return -1;
        }

	do {
		endpointState = kiiPush_retrieveEndpoint() ;
                kiiHal_delayMs(1000);
	}while((endpointState == KIIPUSH_ENDPOINT_UNAVAILABLE));

	if (endpointState == KIIPUSH_ENDPOINT_READY)
	{
	    
		kiiHal_taskCreate(NULL,
			                        kiiPush_task,
						(void *)callback,
						(void *)mKiiPush_taskStk,
						KIIPUSH_TASK_STK_SIZE * sizeof(unsigned char), 
						taskPrio);
	    
		return 0;
	}
	else
	{
	    return -1;
	}
}

