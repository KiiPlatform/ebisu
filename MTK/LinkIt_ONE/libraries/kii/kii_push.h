#ifndef KII_PUSH_H
#define KII_PUSH_H

#ifdef __cplusplus
extern "C" {
#endif

#define KII_PUSH_PING_ENABLE 1
#define KII_PUSH_INSTALLATIONID_SIZE 64
#define KII_PUSH_HOST_SIZE 128
#define KII_PUSH_MQTTTOPIC_SIZE 64
#define KII_PUSH_USERNAME_SIZE 128
#define KII_PUSH_PASSWORD_SIZE 128
#define KII_PUSH_SOCKET_BUF_SIZE 1024
#define KII_PUSH_TOPIC_HEADER_SIZE 8

#if(KII_PUSH_PING_ENABLE)
#define KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE 30
#else
#define KII_PUSH_KEEP_ALIVE_INTERVAL_VALUE 0
#endif

typedef enum
{
    KIIPUSH_ENDPOINT_READY = 0,
    KIIPUSH_ENDPOINT_UNAVAILABLE = 1,
    KIIPUSH_ENDPOINT_ERROR = 2
} kiiPush_endpointState_e;

typedef struct
{
	char installationID[KII_PUSH_INSTALLATIONID_SIZE + 1];
	char mqttTopic[KII_PUSH_MQTTTOPIC_SIZE + 1];
	char host[KII_PUSH_HOST_SIZE + 1];
	char username[KII_PUSH_USERNAME_SIZE + 1];
	char password[KII_PUSH_PASSWORD_SIZE + 1];
	char rcvdBuf[KII_PUSH_SOCKET_BUF_SIZE];
	int mqttSocket;
	char connected;
} kii_push_struct;


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
void kiiPush_recvMsg(void);
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
void kiiPush_pingReq(void);
#endif

#ifdef __cplusplus
}
#endif


#endif

