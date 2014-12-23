#ifndef KII_H
#define KII_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>


#define KII_SITE_SIZE 2
#define KII_HOST_SIZE 64
#define KII_APPID_SIZE 8
#define KII_APPKEY_SIZE 32

#define KII_ACCESS_TOKEN_SIZE   44
#define KII_DEVICE_VENDOR_ID     64 //matches [a-zA-Z0-9-_\\.]{3,64}
#define KII_PASSWORD_SIZE        50  //Matches ^[\\u0020-\\u007E]{4,50}
#define KII_OBJECTID_SIZE 36
#define KII_DATA_TPYE_SIZE 36
#define KII_UPLOAD_ID_SIZE 46
#define KII_BUCKET_NAME_SIZE 64

#define KII_SEND_BUF_SIZE 2048
#define KII_RECV_BUF_SIZE 2048


typedef void (* kiiPush_recvMsgCallback)(char* jsonBuf, int rcvdCounter);



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
extern int kii_init(char *site, char *appID, char *appKey);


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
extern int kiiDev_getToken(char *deviceVendorID, char *password);


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
extern int kiiDev_register(char *vendorDeviceID, char *deviceType, char *password);


/*****************************************************************************
*
*  kiiObj_create
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the output of objectID
*
*  \return 0:success; -1: failure
*
*  \brief  Creates object
*
*****************************************************************************/
extern int kiiObj_create(char *bucketName, char *jsonObject, char *dataType, char *objectID);


/*****************************************************************************
*
*  kiiObj_createWithID
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Creates a new object with an ID
*
*****************************************************************************/
extern int kiiObj_createWithID(char *bucketName, char *jsonObject, char *dataType, char *objectID);


/*****************************************************************************
*
*  kiiObj_fullyUpdate
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Fully updates an object
*
*****************************************************************************/
extern int kiiObj_fullyUpdate(char *bucketName, char *jsonObject, char *dataType, char *objectID);


/*****************************************************************************
*
*  kiiObj_partiallyUpdate
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Partially updates an object
*
*****************************************************************************/
extern int kiiObj_partiallyUpdate(char *bucketName, char *jsonObject, char *objectID);


/*****************************************************************************
*
*  kiiObj_uploadBodyAtOnce
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               dataType - the input of data type, the format should be like "image/jpg"
*               data - raw data
*               length - raw data length
*
*  \return 0:success; -1: failure
*
*  \brief  Uploads object body at once
*
*****************************************************************************/
extern int kiiObj_uploadBodyAtOnce(char *bucketName, char *objectID,  char *dataType, unsigned char *data, unsigned int length);


/*****************************************************************************
*
*  kiiObj_uploadBodyInit
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               dataType - the input of data type, the format should be like "image/jpg"
*               totalLength - the total of data length
*
*  \return 0:success; -1: failure
*
*  \brief  Initializes "uploading an object body in multiple pieces"
*
*****************************************************************************/
extern int kiiObj_uploadBodyInit(char *bucketName, char *objectID, char *dataType, unsigned int totalLength);


/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param: data - the piece of data to be uploaded
*               length - the piece of data length
*
*  \return 0:success; -1: failure
*
*  \brief  Uploads a piece of data
*
*****************************************************************************/
extern int kiiObj_uploadBody(unsigned char *data, unsigned int length);


/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param: committed - 0: cancelled; 1: committed
*
*  \return 0:success; -1: failure
*
*  \brief  Commits or cancels this uploading
*
*****************************************************************************/
extern int kiiObj_uploadBodyCommit(int committed);


/*****************************************************************************
*
*  kiiObj_retrieve
*
*  \param  bucketName - the input of bucket name
*               objectID - the input of objectID
*               jsonObject - the output of object with json format
*               length - the buffer length of jsonObject
*
*  \return 0:success; -1: failure
*
*  \brief  Retrieves object with objectID
*
*****************************************************************************/
extern int kiiObj_retrieve(char *bucketName, char *objectID,  char *jsonObject, unsigned int length);


/*****************************************************************************
*
*  kiiObj_downloadBody
*
*  \param  bucketName - the input of bucket name
*               objectID - the input of objectID
*               position - the downloading position of body
*               length - the downloading length of body
*               data - the output data of received body
*               actualLength - the actual length of received body
*               totalLength - the output of total body length
*
*  \return 0:success; -1: failure
*
*  \brief  Downloads an object body in multiple pieces
*
*****************************************************************************/
extern int kiiObj_downloadBody(char *bucketName, char *objectID,  unsigned int position,  unsigned int length, unsigned char *data, unsigned int *actualLength, unsigned int *totalLength);


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
extern int kiiPush_subscribeAppBucket(char *bucketID);


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
int kiiPush_subscribeThingBucket(char *bucketID);



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
extern int kiiPush_subscribeTopic(char *topicID);


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
int kiiPush_createTopic(char *topicID);



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
int KiiPush_init(unsigned int taskPrio, unsigned int pingReqTaskPrio, kiiPush_recvMsgCallback callback);

#endif

