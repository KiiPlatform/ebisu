#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"
#include "kii_mqtt.h"

extern kii_data_struct g_kii_data;
kii_push_struct g_kii_push;

#define    KIIPUSH_TASK_STK_SIZE      1024
#define    KIIPUSH_PINGREQ_TASK_STK_SIZE      100

static unsigned int  mKiiPush_taskStk[KIIPUSH_TASK_STK_SIZE];     
static unsigned int  mKiiPush_pingReqTaskStk[KIIPUSH_PINGREQ_TASK_STK_SIZE];     



/*****************************************************************************
*
*  kiiPush_install
*
*  \param: none
*
*  \return 0:success; -1: failure
*
*  \brief  Registers installation of a device
*
*****************************************************************************/
static int kiiPush_install(void)
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
   // Json object
   memset(jsonBuf, 0, sizeof(jsonBuf));
   strcpy(jsonBuf, "{\"installationType\":\"MQTT\", \"development\":false}");
   
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

    if ((strstr(buf, "HTTP/1.1 201") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
        return -1;    
    }
    p1 = strstr(buf, "\"installationID\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(g_kii_push.installationID, 0, KII_PUSH_INSTALLATIONID_SIZE+1);
    memcpy(g_kii_push.installationID, p1, p2-p1);

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
*  \brief  Retrieves MQTT endpoint
*
*****************************************************************************/
static kiiPush_endpointState_e kiiPush_retrieveEndpoint(void)
{
    char * p1;
    char * p2;
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_GET);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/installations/");
    strcpy(buf+strlen(buf), g_kii_push.installationID);
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
        return KIIPUSH_ENDPOINT_ERROR;
    }
    buf = g_kii_data.rcvdBuf;

    if ((strstr(buf, "HTTP/1.1 200") != NULL) && (strstr(buf, "{") != NULL) && (strstr(buf, "}") != NULL) )
    {
        //get username
        p1 = strstr(buf, "\"username\"");
        p1 = strstr(p1, ":");
        p1 = strstr(p1, "\"");
        p1 +=1;
        p2 = strstr(p1, "\"");
        memset(g_kii_push.username, 0, KII_PUSH_USERNAME_SIZE+1);
        memcpy(g_kii_push.username, p1, p2-p1);

	//get password
	p1 = strstr(buf, "\"password\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 +=1;
	p2 = strstr(p1, "\"");
	memset(g_kii_push.password, 0, KII_PUSH_PASSWORD_SIZE+1);
	memcpy(g_kii_push.password, p1, p2-p1);
    
	//get host
	p1 = strstr(buf, "\"host\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 +=1;
	p2 = strstr(p1, "\"");
	memset(g_kii_push.host, 0, KII_PUSH_HOST_SIZE+1);
	memcpy(g_kii_push.host, p1, p2-p1);

	 //get mqttTopic
	 p1 = strstr(buf, "\"mqttTopic\"");
	 p1 = strstr(p1, ":");
	 p1 = strstr(p1, "\"");
	 p1 +=1;
	 p2 = strstr(p1, "\"");
	 memset(g_kii_push.mqttTopic, 0, KII_PUSH_MQTTTOPIC_SIZE+1);
	 memcpy(g_kii_push.mqttTopic, p1, p2-p1);
	 return KIIPUSH_ENDPOINT_READY;
    }
    else if (strstr(buf, "HTTP/1.1 503") != NULL)
    {
	 return KIIPUSH_ENDPOINT_UNAVAILABLE;
    }
    else
    {
	return KIIPUSH_ENDPOINT_ERROR;
    }
}



/*****************************************************************************
*
*  kiiPush_subscribeAppBucket
*
*  \param: bucketID - the bucket ID
*
*  \return 0:success; -1: failure
*
*  \brief  Subscribes app scope bucket
*
*****************************************************************************/
int kiiPush_subscribeAppBucket(char *bucketID)
{
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

    if ((strstr(buf, "HTTP/1.1 204") != NULL)  || (strstr(buf, "HTTP/1.1 409") != NULL))
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
*  kiiPush_subscribeThingBucket
*
*  \param: bucketID - the bucket ID
*
*  \return 0:success; -1: failure
*
*  \brief  Subscribes thing scope bucket
*
*****************************************************************************/
int kiiPush_subscribeThingBucket(char *bucketID)
{
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
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

    if ((strstr(buf, "HTTP/1.1 204") != NULL)  || (strstr(buf, "HTTP/1.1 409") != NULL))
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
*  \brief  Subscribes thing scope topic
*
*****************************************************************************/
int kiiPush_subscribeTopic(char *topicID)
{
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
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

    if ((strstr(buf, "HTTP/1.1 204") != NULL)  || (strstr(buf, "HTTP/1.1 409") != NULL))
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
*  kiiPush_createTopic
*
*  \param: topicID - the topic ID
*
*  \return 0:success; -1: failure
*
*  \brief  Creates thing scope topic
*
*****************************************************************************/
int kiiPush_createTopic(char *topicID)
{
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_PUT);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/topics/");
    strcpy(buf+strlen(buf),topicID);
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

    if ((strstr(buf, "HTTP/1.1 204") != NULL)  || (strstr(buf, "HTTP/1.1 409") != NULL))
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
*  kiiPush_recvMsgTask
*
*  \param: sdata - an optional data, points to callback function
*
*  \return none
*
*  \brief  Receives message task
*
*****************************************************************************/
static void *kiiPush_recvMsgTask(void *sdata)
{
    kiiPush_recvMsgCallback callback;

    callback = (kiiPush_recvMsgCallback) sdata;

//    KII_DEBUG("kii-info: installationID:%s\r\n", g_kii_push.installationID);
//    KII_DEBUG("kii-info: mqttTopic:%s\r\n", g_kii_push.mqttTopic);
//    KII_DEBUG("kii-info: host:%s\r\n", g_kii_push.host);
//    KII_DEBUG("kii-info: username:%s\r\n", g_kii_push.username);
//    KII_DEBUG("kii-info: password:%s\r\n", g_kii_push.password);

    g_kii_push.connected = 0;
    for(;;)
    {
        if (g_kii_push.connected == 0)
        {
            kiiHal_delayMs(1000);
            if (KiiMQTT_connect(KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE) < 0)
            {
                continue;
            }
            else if (KiiMQTT_subscribe(QOS1) < 0)
            {
                continue;
            }
            else
            {
               g_kii_push.connected = 1;
            }
        }
        else
        {
            memset(g_kii_push.rcvdBuf, 0, KII_PUSH_RECV_BUF_SIZE);
            g_kii_push.rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, KII_PUSH_RECV_BUF_SIZE);
            if (g_kii_push.rcvdCounter > 0)
            {
                if ((g_kii_push.rcvdBuf[0]&0xf0) == 0x30)
                {
                    int remainingLen;
                    int byteLen;
                    int topicLen;
                    char *p;
                    byteLen = KiiMQTT_decode(&g_kii_push.rcvdBuf[1], &remainingLen);
                    //KII_DEBUG("decode byteLen=%d, remainingLen=%d\r\n", byteLen, remainingLen);
                    p = g_kii_push.rcvdBuf;
                    if (( g_kii_push.rcvdCounter >= remainingLen+byteLen+1) && (byteLen > 0)) //fixed head byte1+remaining length bytes + remaining bytes
                    {
                        p++; //skip fixed header byte1
                        p +=byteLen; //skip remaining length bytes
                        topicLen = p[0] *256 + p[1]; //get topic length
                        p=p+2; //skip 2 topic length bytes
                        p = p+topicLen; //skip topic
                        callback(p, remainingLen-2-topicLen);
                    }
                 }
                else if ((g_kii_push.rcvdBuf[0]&0xf0) == 0xd0)
                {
                    //KII_DEBUG("ping resp\r\n");
                }
            }
	     else
	     { 
                g_kii_push.connected = 0;
	     }
        }
    }
}


/*****************************************************************************
*
*  kiiPush_pingReqTask
*
*  \param: sdata - an optional data, points to callback function
*
*  \return none
*
*  \brief  "PINGREQ" task
*
*****************************************************************************/
static void *kiiPush_pingReqTask(void *sdata)
{
    for(;;)
    {
        if (g_kii_push.connected == 1)
        {
            KiiMQTT_pingReq();
        }		
       kiiHal_delayMs(KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE*1000);
    }
}


/*****************************************************************************
*
*  KiiPush_init
*
*  \param: recvMsgtaskPrio - the priority of task for receiving message
*               pingReqTaskPrio - the priority of task for "PINGREQ" task
*               callback - the call back function for processing the push message received
*
*  \return 0:success; -1: failure
*
*  \brief  Initializes push
*
*****************************************************************************/
int KiiPush_init(unsigned int recvMsgtaskPrio, unsigned int pingReqTaskPrio, kiiPush_recvMsgCallback callback)
{
	kiiPush_endpointState_e endpointState;

    memset(&g_kii_push, 0, sizeof(g_kii_push));
    g_kii_push.mqttSocket = -1;

    if (kiiPush_install() != 0)
    {
        KII_DEBUG("kii-error: push installation failed !\r\n");
        return -1;
    }
	
    do {
        kiiHal_delayMs(1000);
        endpointState = kiiPush_retrieveEndpoint() ;
    }while((endpointState == KIIPUSH_ENDPOINT_UNAVAILABLE));

    if (endpointState == KIIPUSH_ENDPOINT_READY)
    {
        kiiHal_taskCreate(NULL,
			                      kiiPush_recvMsgTask,
						(void *)callback,
						(void *)mKiiPush_taskStk,
						KIIPUSH_TASK_STK_SIZE * sizeof(unsigned char), 
						recvMsgtaskPrio);
	    
        kiiHal_taskCreate(NULL,
			                     kiiPush_pingReqTask,
						NULL,
						(void *)mKiiPush_pingReqTaskStk,
						KIIPUSH_PINGREQ_TASK_STK_SIZE * sizeof(unsigned char), 
						pingReqTaskPrio);
        return 0;
    }
    else
    {
        return -1;
    }
}


