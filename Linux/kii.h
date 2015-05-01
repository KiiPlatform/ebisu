#ifndef KII_H
#define KII_H

#include "kii-core/kii.h"

#define KII_SITE_SIZE 2
#define KII_HOST_SIZE 64
#define KII_APPID_SIZE 8
#define KII_APPKEY_SIZE 32

#define KII_ACCESS_TOKEN_SIZE 44
#define KII_DEVICE_VENDOR_ID 64 /* matches [a-zA-Z0-9-_\\.]{3,64} */
#define KII_PASSWORD_SIZE 50    /* Matches ^[\\u0020-\\u007E]{4,50} */
#define KII_OBJECTID_SIZE 36
#define KII_DATA_TPYE_SIZE 36
#define KII_UPLOAD_ID_SIZE 46
#define KII_BUCKET_NAME_SIZE 64

#define KII_SOCKET_BUF_SIZE 2048

#define DEMO_KII_PUSH_RECV_MSG_TASK_PRIO 3
#define DEMO_KII_PUSH_PINGREQ_TASK_PRIO 4

#ifdef DEBUG
#ifndef __FILE__
#define __FILE__ ("__FILE__ macro is not available")
#endif

#ifndef __LINE__
#define __LINE__ (-1)
#endif

#define M_KII_LOG(x) \
    if (kii->logger_cb != NULL) {\
        kii->logger_cb("file:%s, line:%d ", __FILE__, __LINE__); \
        (x); \
    }
#else
#define M_KII_LOG(x)
#endif

/** Initializes Kii SDK
 *  \param kii sdk instance.
 *  \param site the input of site name, should be one of "CN", "JP", "US", "SG"
 *  \param appID the input of Application ID
 *  \param appKey the input of Application Key
 *  \return  0:success; -1: failure
 */
extern int kii_init(kii_t* kii, char* site, char* appID, char* appKey);

/*****************************************************************************
*
*  kiiDev_getToken
*
*  \param kii - sdk instance.
*               vendorDeviceID - the input of identification of the device
*               password - the input of password
*
*  \return 0:success; -1: failure
*
*  \brief  Gets token
*
*****************************************************************************/
extern int kiiDev_getToken(kii_t* kii, char* deviceVendorID, char* password);

/*****************************************************************************
*
*  kiiDev_register
*
*  \param kii - sdk instance.
*               vendorDeviceID - the input of identification of the device
*               deviceType - the input of device type
*               password - the input of password
*
*  \return 0:success; -1: failure
*
*  \brief  Registers device
*
*****************************************************************************/
extern int kiiDev_register(kii_t* kii, char* vendorDeviceID, char* deviceType, char* password);

/*****************************************************************************
*
*  kiiDev_getIPAddress
*
*  \param  ipAddress - the info of IP address
*
*  \return 0:success; -1: failure
*
*  \brief  Gets external IP address
*
*****************************************************************************/
int kiiDev_getIPAddress(char* ipAddress);

/*****************************************************************************
*
*  kiiObj_create
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the output of objectID
*
*  \return 0:success; -1: failure
*
*  \brief  Creates object
*
*****************************************************************************/
extern int kiiObj_create(	kii_t* kii,
			const kii_bucket_t* bucket,
			const char* object_data,
			const char* object_content_type,
			char* object_id);

/*****************************************************************************
*
*  kiiObj_createWithID
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Creates a new object with an ID
*
*****************************************************************************/
extern int kiiObj_createWithID(
		kii_t* kii,
		const kii_bucket_t* bucket,
		const char* object_id,
		const char* object_data,
		const char* object_content_type);

/*****************************************************************************
*
*  kiiObj_fullyUpdate
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Fully updates an object
*
*****************************************************************************/
extern int kiiObj_patch(
			kii_t* kii,
			const kii_bucket_t* bucket,
			const char* object_id,
			const char* patch_data,
			const char* opt_etag);

extern int kiiObj_delete(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id);


/*****************************************************************************
*
*  kiiObj_partiallyUpdate
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               objectID - the input of objectID
*
*  \return  0:success; -1: failure
*
*  \brief  Partially updates an object
*
*****************************************************************************/
extern int kiiObj_get(
			kii_t* kii,
			const kii_bucket_t* bucket,
			const char* object_id,
			char* object_data,
			size_t size);

/*****************************************************************************
*
*  kiiObj_uploadBodyAtOnce
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
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
extern int kiiObj_uploadBodyAtOnce(int scope,
                                   char* bucketName,
                                   char* objectID,
                                   char* dataType,
                                   unsigned char* data,
                                   unsigned int length);

/*****************************************************************************
*
*  kiiObj_uploadBodyInit
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               objectID - the input of objectID
*               uploadID - the output of uploadID
*
*  \return 0:success; -1: failure
*
*  \brief  Initializes "uploading an object body in multiple pieces"
*
*****************************************************************************/
extern int kiiObj_uploadBodyInit(int scope, char* bucketName, char* objectID, char* uploadID);

/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               objectID - the input of objectID
*               uploadID - the input of uploadID
*               dataType - the input of data type, the format should be like "image/jpg"
*               position - data position
*               length - this  piece of data length
*               totalLength - the total object body length
*               data - raw data
*
*  \return 0:success; -1: failure
*
*  \brief  Uploads a piece of data
*
*****************************************************************************/
extern int kiiObj_uploadBody(int scope,
                             char* bucketName,
                             char* objectID,
                             char* uploadID,
                             char* dataType,
                             unsigned int position,
                             unsigned int length,
                             unsigned int totalLength,
                             unsigned char* data);

/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               objectID - the input of objectID
*               uploadID - the input of uploadID
*               committed - 0: cancelled; 1: committed
*
*  \return 0:success; -1: failure
*
*  \brief  Commits or cancels this uploading
*
*****************************************************************************/
extern int kiiObj_uploadBodyCommit(int scope, char* bucketName, char* objectID, char* uploadID, int committed);

/*****************************************************************************
*
*  kiiObj_downloadBodyAtOnce
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               objectID - the input of objectID
*               data - raw data
*               length - the buffer lengh for object body
*               actualLength - the actual length of received body
*  \return 0:success; -1: failure
*
*  \brief  Downloads an object body at once
*
*****************************************************************************/
extern int kiiObj_downloadBodyAtOnce(int scope,
                                     char* bucketName,
                                     char* objectID,
                                     unsigned char* data,
                                     unsigned int length,
                                     unsigned int* actualLength);

/*****************************************************************************
*
*  kiiObj_downloadBody
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
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
extern int kiiObj_downloadBody(int scope,
                               char* bucketName,
                               char* objectID,
                               unsigned int position,
                               unsigned int length,
                               unsigned char* data,
                               unsigned int* actualLength,
                               unsigned int* totalLength);

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
extern int kiiPush_subscribeBucket(kii_t* kii, kii_bucket_t* bucket);

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
extern int kiiPush_subscribeTopic(kii_t* kii, kii_topic_t* topic);

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
extern int kiiPush_createTopic(kii_t* kii, kii_topic_t* topic);

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
extern int kiiPush_init(kii_t* kii, unsigned int taskPrio, unsigned int pingReqTaskPrio, KII_PUSH_RECEIVED_CB callback);

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
extern int kiiExt_extension(char* endpointName, char* jsonObject);


/* TODO: decide whether we expose it or not */
extern int kiiPush_install(kii_t* kii, kii_bool_t development, char* installation_id);
#endif
