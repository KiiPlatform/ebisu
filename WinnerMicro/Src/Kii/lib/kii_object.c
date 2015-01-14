#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_object.h"
#include "kii_hal.h"

extern kii_data_struct g_kii_data;

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

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
    if ((strlen(buf)+strlen(jsonObject)+1) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	kiiHal_free(buf);
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);
   strcpy(buf+strlen(buf), STR_LF);
   
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if ((strstr(buf, "HTTP/1.1 201") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
	kiiHal_free(buf);
        return -1;    
    }

    p1 = strstr(buf, "\"objectID\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(objectID, 0, KII_OBJECTID_SIZE+1);
    memcpy(objectID, p1, p2-p1);
	
    kiiHal_free(buf);
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

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
    if ((strlen(buf)+strlen(jsonObject)+1) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow!\r\n");
	kiiHal_free(buf);
        return -1;
    }
   strcpy(buf+strlen(buf), jsonObject);
   strcpy(buf+strlen(buf), STR_LF);

    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if ((strstr(buf, "HTTP/1.1 200")  != NULL) || (strstr(buf, "HTTP/1.1 201")  != NULL))
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

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
    if ((strlen(buf)+length) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	kiiHal_free(buf);
        return -1;
    }
    memcpy(buf+strlen(buf), data, length);
    //memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }
    if ((strstr(buf, "HTTP/1.1 200")) != NULL)
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
*  kiiObj_uploadBodyInit
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               uploadID - the output of uploadID
*
*  \return 0:success; -1: failure
*
*  \brief  Initializes "uploading an object body in multiple pieces"
*
*****************************************************************************/
int kiiObj_uploadBodyInit(char *bucketName, char *objectID, char *uploadID)
{
    char * p1;
    char * p2;
    char *buf;

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf), bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf), objectID);
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
   
    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if ((strstr(buf, "HTTP/1.1 200") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
	kiiHal_free(buf);
        return -1;    
    }

    p1 = strstr(buf, "\"uploadID\"");
    p1 = strstr(p1, ":");
    p1 = strstr(p1, "\"");
    p1 +=1;
    p2 = strstr(p1, "\"");
    memset(uploadID, 0, KII_UPLOAD_ID_SIZE+1);
    memcpy(uploadID, p1, p2-p1);

    kiiHal_free(buf);
    return 0;
}


/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param: bucketName - the input of bucket name
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
int kiiObj_uploadBody(char *bucketName, char *objectID, char *uploadID, char *dataType, unsigned int position,  unsigned int length, unsigned int totalLength, unsigned char *data)
{
    char *buf;
	
    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
    strcpy(buf, STR_PUT);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf), bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf), objectID);
    strcpy(buf+strlen(buf), "/body/uploads/");
    strcpy(buf+strlen(buf), uploadID);
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
   strcpy(buf+strlen(buf), dataType);
   strcpy(buf+strlen(buf), STR_CRLF);
   //content-range
   strcpy(buf+strlen(buf), STR_CONTENT_RANGE);
   strcpy(buf+strlen(buf), "bytes=");
   sprintf(buf+strlen(buf), "%d", position);
   strcpy(buf+strlen(buf), "-");
   sprintf(buf+strlen(buf), "%d", position+length-1);
   strcpy(buf+strlen(buf), "/");
   sprintf(buf+strlen(buf), "%d", totalLength);
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
    if ((strlen(buf)+length ) > KII_SOCKET_BUF_SIZE)
    {
        KII_DEBUG("kii-error: buffer overflow !\r\n");
	kiiHal_free(buf);
        return -1;
    }
    memcpy(buf+strlen(buf), data, length);
    //memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);

    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if (strstr(buf, "HTTP/1.1 204")  != NULL)
    {
	kiiHal_free(buf);
	return 0;
    }
    else
    {
	KII_DEBUG("kii-error: upload body failed !\r\n");
	kiiHal_free(buf);
	return -1;
    }
}


/*****************************************************************************
*
*  kiiObj_uploadBody
*
*  \param: bucketName - the input of bucket name
*               objectID - the input of objectID
*               uploadID - the input of uploadID
*               committed - 0: cancelled; 1: committed
*
*  \return 0:success; -1: failure
*
*  \brief  Commits or cancels this uploading
*
*****************************************************************************/
int kiiObj_uploadBodyCommit(char *bucketName, char *objectID, char *uploadID, int committed)
{
    char *buf;
	
    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
    strcpy(buf, STR_POST);
    // url
    strcpy(buf+strlen(buf), "/api/apps/");
    strcpy(buf+strlen(buf), g_kii_data.appID);
    strcpy(buf+strlen(buf), "/things/VENDOR_THING_ID:");
    strcpy(buf+strlen(buf), g_kii_data.vendorDeviceID);
    strcpy(buf+strlen(buf), "/buckets/");
    strcpy(buf+strlen(buf), bucketName);
    strcpy(buf+strlen(buf), "/objects/");
    strcpy(buf+strlen(buf), objectID);
    strcpy(buf+strlen(buf), "/body/uploads/");
    strcpy(buf+strlen(buf), uploadID);
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

    if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
    {
        KII_DEBUG("kii-error: transfer data error !\r\n");
        kiiHal_free(buf);
        return -1;
    }

    if (strstr(buf, "HTTP/1.1 204")  != NULL)
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

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
   
   if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
   {
	   KII_DEBUG("kii-error: transfer data error !\r\n");
	   kiiHal_free(buf);
	   return -1;
   }

    if ((strstr(buf, "HTTP/1.1 200") == NULL) || (strstr(buf, "{") == NULL) || (strstr(buf, "}") == NULL) )
    {
	kiiHal_free(buf);
        return -1;    
    }

    p1 = strstr(buf, "{");
    p2 = strstr(buf, "}");
    p2++;
    if ((p2-p1) > length)
    {
        KII_DEBUG("kii-error: jsonObjectBuf overflow !\r\n");
	kiiHal_free(buf);
	return -1;
    }
    else
    {
        memset(jsonObject, 0, length);
        memcpy(jsonObject, p1, p2-p1);
	kiiHal_free(buf);
        return 0;
    }
}

/*****************************************************************************
*
*  kiiObj_downloadBodyAtOnce
*
*  \param  bucketName - the input of bucket name
*               objectID - the input of objectID
*               data - raw data
*               length - the buffer lengh for object body
*               actualLength - the actual length of received body
*  \return 0:success; -1: failure
*
*  \brief  Downloads an object body at once
*
*****************************************************************************/
int kiiObj_downloadBodyAtOnce(char *bucketName, char *objectID, unsigned char *data, unsigned int length, unsigned int *actualLength)
{
    char * p1;
    char *buf;
    unsigned long contentLengh;

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   
   if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
   {
	   KII_DEBUG("kii-error: transfer data error !\r\n");
	   kiiHal_free(buf);
	   return -1;
   }

    if (strstr(buf, "HTTP/1.1 200") == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }

    p1 = strstr(buf, STR_CONTENT_LENGTH);
    if (p1 == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }
    p1 = p1+strlen(STR_CONTENT_LENGTH);
    contentLengh = atoi(p1);
	
    p1 = strstr(buf, STR_CRLFCRLF);
    if (p1 == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }
    p1 +=4;	
	
    if (contentLengh > length)
    {
        KII_DEBUG("kii-error: the buffer of object body is too small\r\n");
	kiiHal_free(buf);
        return -1;
    }
    else
    {
        memset(data, 0, length);
        memcpy(data, p1, contentLengh);
        *actualLength = contentLengh;
	kiiHal_free(buf);
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
    unsigned long contentLengh;

    buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
    if (buf == NULL)
    {
        KII_DEBUG("kii-error: memory allocation failed !\r\n");
        return -1;
    }
    memset(buf, 0, KII_SOCKET_BUF_SIZE);
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
    sprintf(buf+strlen(buf), "%d", position+length-1);
   strcpy(buf+strlen(buf), STR_CRLF);
   //Authorization
    strcpy(buf+strlen(buf), STR_AUTHORIZATION);
    strcpy(buf+strlen(buf),  " Bearer ");
    strcpy(buf+strlen(buf), g_kii_data.accessToken); 
   strcpy(buf+strlen(buf), STR_CRLF);
   strcpy(buf+strlen(buf), STR_CRLF);
   
   if (kiiHal_transfer(buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
   {
	   KII_DEBUG("kii-error: transfer data error !\r\n");
	   kiiHal_free(buf);
	   return -1;
   }

    if (strstr(buf, "HTTP/1.1 206") == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }

    p1 = strstr(buf, STR_CONTENT_RANGE);
    p1 = strstr(p1, "/");
    if (p1 == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }
    p1++;
    *totalLength = atoi(p1);

    p1 = strstr(buf, STR_CONTENT_LENGTH);
    if (p1 == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }
    p1 = p1+strlen(STR_CONTENT_LENGTH);
    contentLengh = atoi(p1);
	
    p1 = strstr(buf, STR_CRLFCRLF);
    if (p1 == NULL)
    {
	kiiHal_free(buf);
	 return -1;
    }
    p1 +=4;	

    if (contentLengh > length)
    {
        KII_DEBUG("kii-error: the buffer of object body is too small");
	kiiHal_free(buf);
        return -1;
    }
    else
    {
        memset(data, 0, length);
        memcpy(data, p1, contentLengh);
        *actualLength = contentLengh;
	kiiHal_free(buf);
        return 0;
    }
}


