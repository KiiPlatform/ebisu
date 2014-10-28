#ifndef KII_H
#define KII_H

#define KII_SITE_SIZE 2
#define KII_HOST_SIZE 64
#define KII_APPID_SIZE 8
#define KII_APPKEY_SIZE 32

#define KII_ACCESS_TOKEN_SIZE   44
#define KII_DEVICE_VENDOR_ID     64 //matches [a-zA-Z0-9-_\\.]{3,64}
#define KII_DEVICE_ID                  20
#define KII_PASSWORD_SIZE        50  //Matches ^[\\u0020-\\u007E]{4,50}
#define KII_OBJECTID_SIZE 36
#define KII_DATA_TPYE_SIZE 36
#define KII_UPLOAD_ID_SIZE 46
#define KII_BUCKET_NAME_SIZE 64

#define KII_SEND_BUF_SIZE 2048
#define KII_RECV_BUF_SIZE 2048


typedef void (* kiiPush_recvMessageCallback)(char* jsonBuf, int rcvdCounter);



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
*  \brief  Kii initialize
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
*  \brief  get token
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
*  \brief  register device
*
*****************************************************************************/
extern int kiiDev_register(char *vendorDeviceID, char *deviceType, char *password);


/*****************************************************************************
*
*  kiiObj_create
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata",  it can be set NULL if object is not an specific data type in the platform
*               objectID - the output of objectID
*
*  \return 0:success; -1: failure
*
*  \brief  create object
*
*****************************************************************************/
extern int kiiObj_create(char *bucketName, char *jsonObject, char *dataType, char *objectID);


/*****************************************************************************
*
*  kiiObj_createWithID
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata",  it can be set NULL if object is not an specific data type in the platform
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  create a new object with an ID
*
*****************************************************************************/
extern int kiiObj_createWithID(char *bucketName, char *jsonObject, char *dataType, char *objectID);


/*****************************************************************************
*
*  kiiObj_fullyUpdate
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata",  it can be set NULL if object is not an specific data type in the platform
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  fully update an object
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
*  \brief  partially update an object
*
*****************************************************************************/
extern int kiiObj_partiallyUpdate(char *bucketName, char *jsonObject, char *objectID);


/*****************************************************************************
*
*  kiiObj_uploadBodyAtOnce
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               dataType - the input of data type, it must not be NULL, the format should be like "image/jpg"
*               data - raw data
*               length - raw data length
*
*  \return 0:success; -1: failure
*
*  \brief  upload object body at once
*
*****************************************************************************/
extern int kiiObj_uploadBodyAtOnce(char *bucketName, char *objectID,  char *dataType, unsigned char *data, unsigned int length);


/*****************************************************************************
*
*  kiiObj_uploadBodyInit
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               dataType - the input of data type, it must not be NULL, the format should be like "image/jpg"
*               totalLength - the total of data length
*
*  \return 0:success; -1: failure
*
*  \brief  init uploading an object body in multiple pieces
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
*  \brief  upload a piece of data
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
*  \brief  commit or cancel this uploading
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
*  \brief  retrieve object with objectID
*
*****************************************************************************/
extern int kiiObj_retrieve(char *bucketName, char *objectID,  char *jsonObject, int length);


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
*               totalLength - the output of total length of body
*
*  \return 0:success; -1: failure
*
*  \brief  download an object in multiple pieces
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
*  \brief  subscribe app scope bucket
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
*  \brief  subscribe thing scope bucket
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
*  \brief  subscribe topic
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
*  \brief  create topic
*
*****************************************************************************/
int kiiPush_createTopic(char *topicID);



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
extern int KiiPush_init(unsigned int taskPrio, kiiPush_recvMessageCallback callback);


#endif

