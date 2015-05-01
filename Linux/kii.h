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
 *  \param [inout] kii sdk instance.
 *  \param [in] site the input of site name, should be one of "CN", "JP", "US", "SG"
 *  \param [in] app_id the input of Application ID
 *  \param [in] app_key the input of Application Key
 *  \return  0:success, -1: failure
 */
int kii_init(
        kii_t* kii,
        const char* site,
        const char* app_id,
        const char* app_key);

/** Authorize thing with vendor thing id and password.
 *  After the authentication, access token is used to call APIs access to
 *  Kii Cloud so that the thing can access private data.
 *  \param [inout] kii sdk instance.
 *  \param [in] vendor_thing_id the thing identifier given by vendor.
 *  \param [in] password the password of the thing given by vendor.
 *  \return 0:success, -1: failure
 */
int kii_thing_authenticate(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* password);

/** Register new thing
 *  After the registtration, access token is used to call APIs access to
 *  Kii Cloud so that the thing can access private data.
 *  \param [inout] kii sdk instance.
 *  \param [in] vendor_thing_id the thing identifier given by vendor.
 *  \param [in] thing_type the type of the thing given by vendor.
 *  \param [in] password - the password of the thing given by vendor.
 *  \return 0:success, -1: failure
 */
int kii_thing_register(
        kii_t* kii,
        const char* vendor_thing_id,
        const char* thing_type,
        const char* password);

/** Create new object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_data key-value pair of the object in json format.
 *  \param [in] object_content_type content-type of the object. If null,
 *  application/json will be applied.
 *  \param [out] object_id output of the created object ID.
 *  Supplied when succeeded to create new object.
 *  Must be allocated by application with the size of KII_OBJECTID_SIZE
 *  before this api call.
 *  \return 0:success, -1: failure
 */
int kii_object_create(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_data,
        const char* object_content_type,
        char* out_object_id);

/** Create new object with the specified ID
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] object_data key-value pair of the object in json format.
 *  \param [in] object_content_type content-type of the object. If null,
 *  application/json will be applied.
 *  \return  0:success, -1: failure
 */
int kii_object_create_with_id(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* object_data,
        const char* object_content_type);

/** Partial update of the object
 *  Only the specified key-value is updated and other key-values won't be
 *  updated/ removed.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] patch_data key-value pair of the object in json format.
 *  \param [in] opt_etag etag of the object. if specified, update will be failed
 *  if there is updates on cloud. if NULL, forcibly updates.
 *  \return  0:success, -1: failure
 */
int kii_object_patch(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag);

/** Full update of the object
 * Replace the object with specified key-values.
 * Existing key-value pair which is not included in the replacement_data will be
 * removed.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \param [in] replacement_data key-value pair of the object in json format.
 *  \param [in] opt_etag etag of the object. if specified, update will be failed
 *  if there is updates on cloud. if NULL, forcibly updates.
 *  \return  0:success, -1: failure
 */
int kii_object_replace(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* replacement_data,
        const char* opt_etag);

/** Delete the object
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  0:success, -1: failure
 */
int kii_object_delete(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id);

/** Get the object
 *  When succeeded, obtained object data is cached in kii_t#response_body.
 *  \param [inout] kii sdk instance.
 *  \param [in] bucket specify the bucket of which object is stored.
 *  \param [in] object_id specify the id of the object.
 *  \return  0:success, -1: failure
 */
int kii_object_get(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id);

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
