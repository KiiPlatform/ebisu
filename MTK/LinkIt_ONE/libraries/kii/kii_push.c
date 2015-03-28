#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_push.h"
#include "kii_mqtt.h"

extern kii_data_struct g_kii_data;
kii_push_struct g_kii_push;
kiiPush_recvMsgCallback m_callback;

#define KIIPUSH_TASK_STK_SIZE 8
static unsigned int mKiiPush_taskStk[KIIPUSH_TASK_STK_SIZE];
#if(KII_PUSH_PING_ENABLE)
#define KIIPUSH_PINGREQ_TASK_STK_SIZE 8
static unsigned int mKiiPush_pingReqTaskStk[KIIPUSH_PINGREQ_TASK_STK_SIZE];
#endif

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
	char* p1;
	char* p2;
	char* buf;
	char jsonBuf[256];

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_POST);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	strcat(buf, "/installations");
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
	strcat(buf, "application/vnd.kii.InstallationCreationRequest+json");
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Json object
	memset(jsonBuf, 0, sizeof(jsonBuf));
	strcpy(jsonBuf, "{\"installationType\":\"MQTT\", \"development\":false}");

	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", strlen(jsonBuf) + 1);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + strlen(jsonBuf) + 1) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	strcat(buf, jsonBuf);
	strcat(buf, STR_LF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if(strstr(buf, "HTTP/1.1 201") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1 = strstr(buf, "\"installationID\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memset(g_kii_push.installationID, 0, KII_PUSH_INSTALLATIONID_SIZE + 1);
	memcpy(g_kii_push.installationID, p1, p2 - p1);

	kiiHal_free(buf);
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
	char* p1;
	char* p2;
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return KIIPUSH_ENDPOINT_ERROR;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_GET);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	strcat(buf, "/installations/");
	strcat(buf, g_kii_push.installationID);
	strcat(buf, "/mqtt-endpoint");
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
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return KIIPUSH_ENDPOINT_ERROR;
	}

	if(strstr(buf, "HTTP/1.1 200") != NULL)
	{
		// get username
		p1 = strstr(buf, "\"username\"");
		p1 = strstr(p1, ":");
		p1 = strstr(p1, "\"");
		p1 += 1;
		p2 = strstr(p1, "\"");
		memset(g_kii_push.username, 0, KII_PUSH_USERNAME_SIZE + 1);
		memcpy(g_kii_push.username, p1, p2 - p1);

		// get password
		p1 = strstr(buf, "\"password\"");
		p1 = strstr(p1, ":");
		p1 = strstr(p1, "\"");
		p1 += 1;
		p2 = strstr(p1, "\"");
		memset(g_kii_push.password, 0, KII_PUSH_PASSWORD_SIZE + 1);
		memcpy(g_kii_push.password, p1, p2 - p1);

		// get host
		p1 = strstr(buf, "\"host\"");
		p1 = strstr(p1, ":");
		p1 = strstr(p1, "\"");
		p1 += 1;
		p2 = strstr(p1, "\"");
		memset(g_kii_push.host, 0, KII_PUSH_HOST_SIZE + 1);
		memcpy(g_kii_push.host, p1, p2 - p1);

		// get mqttTopic
		p1 = strstr(buf, "\"mqttTopic\"");
		p1 = strstr(p1, ":");
		p1 = strstr(p1, "\"");
		p1 += 1;
		p2 = strstr(p1, "\"");
		memset(g_kii_push.mqttTopic, 0, KII_PUSH_MQTTTOPIC_SIZE + 1);
		memcpy(g_kii_push.mqttTopic, p1, p2 - p1);
		kiiHal_free(buf);
		return KIIPUSH_ENDPOINT_READY;
	}
	else if(strstr(buf, "HTTP/1.1 503") != NULL)
	{
		kiiHal_free(buf);
		return KIIPUSH_ENDPOINT_UNAVAILABLE;
	}
	else
	{
		kiiHal_free(buf);
		return KIIPUSH_ENDPOINT_ERROR;
	}
}

/*****************************************************************************
*
*  kiiPush_subscribeBucket
*
*  \param  scope - bucket scope
*               bucketID - the bucket ID
*
*  \return 0:success; -1: failure
*
*  \brief  Subscribes bucket
*
*****************************************************************************/
int kiiPush_subscribeBucket(int scope, char* bucketID)
{
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_POST);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/buckets/");
	strcat(buf, bucketID);
	strcat(buf, "/filters/all/push/subscriptions/things");
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
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if((strstr(buf, "HTTP/1.1 204") != NULL) || (strstr(buf, "HTTP/1.1 409") != NULL))
	{
		kiiHal_free(buf);
		return 0;
	}
	else
	{
		kiiHal_free(buf);
		return -1;
	}
}

/*****************************************************************************
*
*  kiiPush_subscribeTopic
*
*  \param: scope - topic scope
*               topicID - the topic ID
*
*  \return 0:success; -1: failure
*
*  \brief  Subscribes thing scope topic
*
*****************************************************************************/
int kiiPush_subscribeTopic(int scope, char* topicID)
{
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_POST);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/topics/");
	strcat(buf, topicID);
	strcat(buf, "/push/subscriptions/things");
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
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if((strstr(buf, "HTTP/1.1 204") != NULL) || (strstr(buf, "HTTP/1.1 409") != NULL))
	{
		kiiHal_free(buf);
		return 0;
	}
	else
	{
		kiiHal_free(buf);
		return -1;
	}
}

/*****************************************************************************
*
*  kiiPush_createTopic
*
*  \param: scope - topic scope
*               topicID - the topic ID
*
*  \return 0:success; -1: failure
*
*  \brief  Creates thing scope topic
*
*****************************************************************************/
int kiiPush_createTopic(int scope, char* topicID)
{
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_PUT);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/topics/");
	strcat(buf, topicID);
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
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if((strstr(buf, "HTTP/1.1 204") != NULL) || (strstr(buf, "HTTP/1.1 409") != NULL))
	{
		kiiHal_free(buf);
		return 0;
	}
	else
	{
		kiiHal_free(buf);
		return -1;
	}
}

/*****************************************************************************
*
*  kiiPush_recvMsg
*
*  \param: none
*
*  \return none
*
*  \brief  Receives mqtt message
*
*****************************************************************************/
void kiiPush_recvMsg(void)
{
	int remainingLen;
	int byteLen;
	int topicLen;
	int totalLen;
	char* p;
	int bytes;
	int rcvdCounter;
	kiiPush_recvMsgCallback callback;
	kiiPush_endpointState_e endpointState;

	callback = m_callback;
	//for(;;)
	{
		if(g_kii_push.connected == 0)
		{
			if(kiiPush_install() != 0)
			{
				kiiHal_delayMs(1000);
				return;
			}

			do
			{
				kiiHal_delayMs(1000);
				endpointState = kiiPush_retrieveEndpoint();
			}
			while((endpointState == KIIPUSH_ENDPOINT_UNAVAILABLE));

			if(endpointState != KIIPUSH_ENDPOINT_READY)
			{
				return;
			}
			 KII_DEBUG("kii-info: installationID:%s\r\n", g_kii_push.installationID);
			 KII_DEBUG("kii-info: mqttTopic:%s\r\n", g_kii_push.mqttTopic);
			 KII_DEBUG("kii-info: host:%s\r\n", g_kii_push.host);
			 KII_DEBUG("kii-info: username:%s\r\n", g_kii_push.username);
			 KII_DEBUG("kii-info: password:%s\r\n", g_kii_push.password);
			if(KiiMQTT_connect(KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE) < 0)
			{
				return;
			}
			else if(KiiMQTT_subscribe(QOS1) < 0)
			{
				return;
			}
			else
			{
				g_kii_push.connected = 1;
			}
		}
		else
		{
		    if (kiiHal_socketConnected() != 0)
		    {
				KII_DEBUG("kii-error: mqtt disconnected\r\n");
				g_kii_push.connected = 0;
				return;
		    }

			if (kiiHal_socketRecvDataAvailable() != 0)
			{
			    return;
			}
			memset(g_kii_push.rcvdBuf, 0, KII_PUSH_SOCKET_BUF_SIZE);
			rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf, 2);
			if(rcvdCounter == 2)
			{
				if((g_kii_push.rcvdBuf[0] & 0xf0) == 0x30)
				{
					rcvdCounter = kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf+2, KII_PUSH_TOPIC_HEADER_SIZE);
					if(rcvdCounter == KII_PUSH_TOPIC_HEADER_SIZE)
					{
					    byteLen = KiiMQTT_decode(&g_kii_push.rcvdBuf[1], &remainingLen);
					}
					else
					{
						KII_DEBUG("kii-error: mqtt decode error\r\n");
						g_kii_push.connected = 0;
						return;
					}
					if(byteLen > 0)
					{
						totalLen =
						    remainingLen + byteLen + 1; // fixed head byte1+remaining length bytes + remaining bytes
					}
					else
					{
						KII_DEBUG("kii-error: mqtt decode error\r\n");
						g_kii_push.connected = 0;
						return;
					}
					if(totalLen > KII_PUSH_SOCKET_BUF_SIZE)
					{
						KII_DEBUG("kii-error: mqtt buffer overflow\r\n");
						g_kii_push.connected = 0;
						return;
					}

					// KII_DEBUG("decode byteLen=%d, remainingLen=%d\r\n", byteLen, remainingLen);
					bytes = rcvdCounter + 2;
					while(bytes < totalLen)
					{
						rcvdCounter =
						    kiiHal_socketRecv(g_kii_push.mqttSocket, g_kii_push.rcvdBuf + bytes, totalLen - bytes);
						if(rcvdCounter > 0)
						{
							bytes += rcvdCounter;
						}
						else
						{
							bytes = -1;
							break;
						}
					}
					 printf("bytes:%d, totalLen:%d\r\n", bytes, totalLen);
					if(bytes >= totalLen)
					{
						p = g_kii_push.rcvdBuf;
						p++; // skip fixed header byte1
						p += byteLen; // skip remaining length bytes
						topicLen = p[0] * 256 + p[1]; // get topic length
						p = p + 2; // skip 2 topic length bytes
						p = p + topicLen; // skip topic
						if((remainingLen - 2 - topicLen) > 0)
						{
						    callback(p, remainingLen - 2 - topicLen);
						}
						else
						{
							KII_DEBUG("kii-error: mqtt topic length error\r\n");
							g_kii_push.connected = 0;
							return;
						}
					}
					else
					{
						KII_DEBUG("kii_error: mqtt receive data error\r\n");
						g_kii_push.connected = 0;
						return;
					}
				}
#if(KII_PUSH_PING_ENABLE)
				else if((g_kii_push.rcvdBuf[0] & 0xf0) == 0xd0)
				{
					KII_DEBUG("ping resp\r\n");
				}
#endif
			}
			else
			{
				g_kii_push.connected = 0;
				KII_DEBUG("kii-error: mqtt receive data error\r\n");
			}
		}
	}
}
#if(KII_PUSH_PING_ENABLE)
/*****************************************************************************
*
*  kiiPush_pingReq
*
*  \param: none
*
*  \return none
*
*  \brief  mqtt ping request
*
*****************************************************************************/
void kiiPush_pingReq(void)
{
#define MQTT_PING_DELAY_PER_TIME 100 //ms
    static int count = 0;

	if (((count++) * MQTT_PING_DELAY_PER_TIME) >= (KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE*1000))
    {
        if (g_kii_push.connected == 1)
        {
            KiiMQTT_pingReq();
        }
		count = 0;
    }
	kiiHal_delayMs(MQTT_PING_DELAY_PER_TIME);
}
#endif

int KiiPush_init(unsigned int recvMsgtaskPrio, unsigned int pingReqTaskPrio, kiiPush_recvMsgCallback callback)
{
    memset(&g_kii_push, 0, sizeof(g_kii_push));
    g_kii_push.mqttSocket = -1;
    g_kii_push.connected = 0;
    m_callback = callback;
    return 0;
}

