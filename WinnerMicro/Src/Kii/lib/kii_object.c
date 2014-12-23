#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_object.h"
#include "kii_hal.h"

extern kii_data_struct g_kii_data;

static char mBucketName[KII_BUCKET_NAME_SIZE+1];
static char mObjectID[KII_OBJECTID_SIZE+1];
static char mDataType[KII_DATA_TPYE_SIZE+1];
static char mUploadID[KII_UPLOAD_ID_SIZE+1];

static unsigned int mObjBodyTotalLength;
static unsigned int mObjBodyCurrentPosition;
static int mSocketNum;


static int kiiObj_update(char *bucketName, char *jsonObject, char *dataType, char *objectID, int updateOrCreateWithID);


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
int kiiObj_create(char *bucketName, char *jsonObject, char *dataType, char *objectID)
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
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects");
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
   strcpy(buf+strlen(buf), "application/vnd.");
   strcpy(buf+strlen(buf), g_kii_data.appID);
   strcpy(buf+strlen(buf), ".");
   strcpy(buf+strlen(buf), dataType);
   strcpy(buf+strlen(buf), "+json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonObject)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonObject)+1) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);
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

    p1 = strstr(buf, "\"objectID\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(objectID, 0, KII_OBJECTID_SIZE+1);
    memcpy(objectID, p1, p2-p1);
	
    return 0;
}

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
int kiiObj_createWithID(char *bucketName, char *jsonObject, char *dataType, char *objectID)
{
    return kiiObj_update(bucketName, jsonObject, dataType, objectID, KIIOBJ_CREATE_WITH_ID);
}


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
int kiiObj_fullyUpdate(char *bucketName, char *jsonObject, char *dataType, char *objectID)
{
    return kiiObj_update(bucketName, jsonObject, dataType, objectID, KIIOBJ_FULLY_UPDATE);
}

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
int kiiObj_partiallyUpdate(char *bucketName, char *jsonObject, char *objectID)
{
    return kiiObj_update(bucketName, jsonObject, NULL, objectID, KIIOBJ_PARTIALLY_UPDATE);
}

/*****************************************************************************
*
*  kiiObj_fullyUpdate
*
*  \param  bucketName - the input of bucket name
*               jsonObject - the input of object with json format
*               dataType - the input of data type, the format should be like "mydata"
*               objectID - the input of objectID
*               updateOrCreateWithID - kind of "kiiObj_updateType_e" type
*
*  \return  0:success; -1: failure
*
*  \brief  Partially/fully updates an object, or creates a new object with an id
*
*****************************************************************************/
static int kiiObj_update(char *bucketName, char *jsonObject, char *dataType, char *objectID, int updateOrCreateWithID)
{
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    if (updateOrCreateWithID != KIIOBJ_PARTIALLY_UPDATE)
    {
        strcpy(buf, STR_PUT);
    }
    else
    {
        strcpy(buf, STR_POST);
    }
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),objectID);
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
   if (updateOrCreateWithID == KIIOBJ_PARTIALLY_UPDATE)
   {
        //strcpy(buf+strlen(buf),  "If-Match:2");
        //strcpy(buf+strlen(buf), STR_CRLF);
        strcpy(buf+strlen(buf),  "X-HTTP-Method-Override:PATCH");
        strcpy(buf+strlen(buf), STR_CRLF);
   }
   else
   {
       //content-type	
        strcpy(buf+strlen(buf), STR_CONTENT_TYPE);
        strcpy(buf+strlen(buf), "application/vnd.");
        strcpy(buf+strlen(buf), g_kii_data.appID);
        strcpy(buf+strlen(buf), ".");
        strcpy(buf+strlen(buf), dataType);
        strcpy(buf+strlen(buf), "+json");
        strcpy(buf+strlen(buf), STR_CRLF);
		
        if (updateOrCreateWithID == KIIOBJ_FULLY_UPDATE)
        {
            //strcpy(buf+strlen(buf),  "If-Match:1");
            //strcpy(buf+strlen(buf), STR_CRLF);
        }
   }
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(jsonObject)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+strlen(jsonObject)+1) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow!\r\n");
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);
   strcpy(buf+strlen(buf), STR_LF);

    g_kii_data.sendDataLen = strlen(buf);

    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;

    if ((strstr(buf, "HTTP/1.1 200")  != NULL) || (strstr(buf, "HTTP/1.1 201")  != NULL))
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
int kiiObj_uploadBodyAtOnce(char *bucketName, char *objectID,  char *dataType, unsigned char *data, unsigned int length)
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
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),objectID);
    strcpy(buf+strlen(buf), "/body");
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
   strcpy(buf+strlen(buf), dataType);
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", length);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+length) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
        return -1;
    }
    g_kii_data.sendDataLen = strlen(buf) + length;
    memcpy(buf+strlen(buf), data, length);
    //memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);
    if (kiiHal_transfer() != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        return -1;
    }
    buf = g_kii_data.rcvdBuf;
    if ((strstr(buf, "HTTP/1.1 200")) != NULL)
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
int kiiObj_uploadBodyInit(char *bucketName, char *objectID, char *dataType, unsigned int totalLength)
{
    char * p1;
    char * p2;
    char *buf;
    unsigned char ipBuf[4];

    
    memset(mBucketName, 0, sizeof(mBucketName));
    strcpy(mBucketName, bucketName);
    memset(mObjectID, 0, sizeof(mObjectID));
    strcpy(mObjectID, objectID);
    memset(mDataType, 0, sizeof(mDataType));
    strcpy(mDataType, dataType);
    mObjBodyTotalLength = totalLength;
    mObjBodyCurrentPosition = 0;
	
    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),mBucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),mObjectID);
    strcpy(buf+strlen(buf), "/body/uploads");
    strcpy(buf+strlen(buf), STR_HTTP);
   strcpy(buf+strlen(buf), STR_CRLF);
   //accept
   strcpy(buf+strlen(buf), STR_ACCEPT);
   strcpy(buf+strlen(buf), "application/vnd.kii.startobjectbodyuploadresponse+json");
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
   strcpy(buf+strlen(buf), "application/vnd.kii.startobjectbodyuploadrequest+json");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", strlen(STR_EMPTY_JSON)+1);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_EMPTY_JSON);
   strcpy(buf+strlen(buf), STR_LF);
   
    g_kii_data.sendDataLen = strlen(buf);


    if (kiiHal_dns(g_kii_data.host, ipBuf) < 0)
    {
        KII_DEBUG("kii-error: dns failed !\r\n");
        return -1;
    }
		
    mSocketNum = kiiHal_socketCreate();
    if (mSocketNum < 0)
    {
        KII_DEBUG("kii-error: create socket failed !\r\n");
        return -1;
    }
	
	
    if (kiiHal_connect(mSocketNum, (char*)ipBuf, KII_DEFAULT_PORT) < 0)
    {
        KII_DEBUG("kii-error: connect to server failed \r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }
    
    if (kiiHal_socketSend(mSocketNum, g_kii_data.sendBuf, g_kii_data.sendDataLen) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

    memset(g_kii_data.rcvdBuf, 0, KII_RECV_BUF_SIZE);
    g_kii_data.rcvdCounter = kiiHal_socketRecv(mSocketNum, g_kii_data.rcvdBuf, KII_RECV_BUF_SIZE);
    if (g_kii_data.rcvdCounter < 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

    buf = g_kii_data.rcvdBuf;

    if ((strstr(buf, "HTTP/1.1 200") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
	kiiHal_socketClose(&mSocketNum);
        return -1;    
    }

    p1 = strstr(buf, "\"uploadID\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(mUploadID, 0, sizeof(mUploadID));
    memcpy(mUploadID, p1, p2-p1);

    return 0;
}


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
int kiiObj_uploadBody(unsigned char *data, unsigned int length)
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
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),mBucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),mObjectID);
    strcpy(buf+strlen(buf), "/body/uploads/");
    strcpy(buf+strlen(buf), mUploadID);
    strcpy(buf+strlen(buf), "/data");
    strcpy(buf+strlen(buf), STR_HTTP);
   strcpy(buf+strlen(buf), STR_CRLF);
   //accept
   strcpy(buf+strlen(buf), STR_ACCEPT);
   strcpy(buf+strlen(buf), "application/json, application/*+json");
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
   strcpy(buf+strlen(buf), mDataType);
   strcpy(buf+strlen(buf), STR_CRLF);
   //content-range
   strcpy(buf+strlen(buf), STR_CONTENT_RANGE);
   strcpy(buf+strlen(buf), "bytes=");
   sprintf(buf+strlen(buf), "%d", mObjBodyCurrentPosition);
   mObjBodyCurrentPosition +=length;
   strcpy(buf+strlen(buf), "-");
   sprintf(buf+strlen(buf), "%d", mObjBodyCurrentPosition-1);
   strcpy(buf+strlen(buf), "/");
   sprintf(buf+strlen(buf), "%d", mObjBodyTotalLength);
  strcpy(buf+strlen(buf), STR_CRLF);
      //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
    //Content-Length
   strcpy(buf+strlen(buf), STR_CONTENT_LENGTH);
   sprintf(buf+strlen(buf), "%d", length);
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
    if ((strlen(buf)+length ) > KII_SEND_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }
    g_kii_data.sendDataLen = strlen(buf) + length;
    memcpy(buf+strlen(buf), data, length);
    //memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);

    if (kiiHal_socketSend(mSocketNum, g_kii_data.sendBuf, g_kii_data.sendDataLen) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

    memset(g_kii_data.rcvdBuf, 0, KII_RECV_BUF_SIZE);
    g_kii_data.rcvdCounter = kiiHal_socketRecv(mSocketNum, g_kii_data.rcvdBuf, KII_RECV_BUF_SIZE);
    if (g_kii_data.rcvdCounter < 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

    buf = g_kii_data.rcvdBuf;

    if (strstr(buf, "HTTP/1.1 204")  != NULL)
    {
	return 0;
    }
    else
    {
	KII_DEBUG("kii-error: upload body failed !\r\n");
	kiiHal_socketClose(&mSocketNum);
	return -1;
    }
}


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
int kiiObj_uploadBodyCommit(int committed)
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
    strcpy(buf+strlen(buf),mBucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),mObjectID);
    strcpy(buf+strlen(buf), "/body/uploads/");
    strcpy(buf+strlen(buf), mUploadID);
    strcpy(buf+strlen(buf), "/status/");
    if (committed == 1)
    {
        strcpy(buf+strlen(buf), "committed");
    }
    else
    {
        strcpy(buf+strlen(buf), "cancelled");
    }
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

    if (kiiHal_socketSend(mSocketNum, g_kii_data.sendBuf, g_kii_data.sendDataLen) < 0)
    {
        
        KII_DEBUG("kii-error: send data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

    memset(g_kii_data.rcvdBuf, 0, KII_RECV_BUF_SIZE);
    g_kii_data.rcvdCounter = kiiHal_socketRecv(mSocketNum, g_kii_data.rcvdBuf, KII_RECV_BUF_SIZE);
    if (g_kii_data.rcvdCounter < 0)
    {
        KII_DEBUG("kii-error: recv data fail\r\n");
	 kiiHal_socketClose(&mSocketNum);
        return -1;
    }

     kiiHal_socketClose(&mSocketNum);
    buf = g_kii_data.rcvdBuf;

    if (strstr(buf, "HTTP/1.1 204")  != NULL)
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
int kiiObj_retrieve(char *bucketName, char *objectID,  char *jsonObject, unsigned int length)
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
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),objectID);
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
    if ((strstr(buf, "HTTP/1.1 200") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
        return -1;    
    }

    p1 = strstr(buf, "{");
    p2 = strstr(buf, "}");
    p2++;
    if ((p2-p1) > length)
    {
        KII_DEBUG("kii-error: jsonObjectBuf overflow !\r\n");
	return -1;
    }
    else
    {
        memset(jsonObject, 0, length);
        memcpy(jsonObject, p1, p2-p1);
        return 0;
    }
}



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
int kiiObj_downloadBody(char *bucketName, char *objectID,  unsigned int position,  unsigned int length, unsigned char *data, unsigned int *actualLength, unsigned int *totalLength)
{
    char * p1;
    char *buf;

    buf = g_kii_data.sendBuf;
    memset(buf, 0, KII_SEND_BUF_SIZE);
    strcpy(buf, STR_GET);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf),bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf),objectID);
    strcpy(buf+strlen(buf), "/body");
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
   //Accept
    strcpy(buf+strlen(buf), STR_ACCEPT);
   strcpy(buf+strlen(buf), "*/*");
   strcpy(buf+strlen(buf), STR_CRLF);
   //Range
    strcpy(buf+strlen(buf), STR_RANGE);
     strcpy(buf+strlen(buf), "bytes=");
    sprintf(buf+strlen(buf), "%d", position);
    strcpy(buf+strlen(buf), "-");
    sprintf(buf+strlen(buf), "%d", position+length);
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

    if (strstr(buf, "HTTP/1.1 206") == NULL)
    {
	 return -1;
    }

    p1 = strstr(buf, STR_CONTENT_RANGE);
    p1 = strstr(p1, "/");
    if (p1 == NULL)
    {
	 return -1;
    }
    p1++;
    *totalLength = atoi(p1);

    p1 = strstr(buf, STR_CONTENT_LENGTH);
    if (p1 == NULL)
    {
	 return -1;
    }
    p1 = p1+strlen(STR_CONTENT_LENGTH);
    *actualLength = atoi(p1);
	
    p1 = strstr(buf, "\r\n\r\n");
    if (p1 == NULL)
    {
	 return -1;
    }
    p1 +=4;	
    if ((p1+ (*actualLength)) > (buf + KII_RECV_BUF_SIZE))
    {
	KII_DEBUG("kii-error: receiving buffer overflow !\r\n");
	return -1;
    }

{
	int tmp1;
	int tmp2;
	tmp1 = (buf + g_kii_data.rcvdCounter) - p1;
	tmp2 = *actualLength;
	KII_DEBUG("tmp1=%d\r\n", tmp1);
	KII_DEBUG("tmp2=%d\r\n", tmp2);
	
    if (((buf + g_kii_data.rcvdCounter) - p1)  <  (*actualLength))
    {
	KII_DEBUG("kii-error: missing data !\r\n");
	return -1;
    }
}
    memset(data, 0, length);
    memcpy(data, p1, *actualLength);
    return 0;
}


