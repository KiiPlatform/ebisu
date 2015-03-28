#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_object.h"
#include "kii_hal.h"

extern kii_data_struct g_kii_data;

static int
kiiObj_update(int scope, char* bucketName, char* jsonObject, char* dataType, char* objectID, int updateOrCreateWithID);

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
int kiiObj_create(int scope, char* bucketName, char* jsonObject, char* dataType, char* objectID)
{
	char* p1;
	char* p2;
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
	strcat(buf, bucketName);
	strcat(buf, "/objects");
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
	strcat(buf, "application/vnd.");
	strcat(buf, g_kii_data.appID);
	strcat(buf, ".");
	strcat(buf, dataType);
	strcat(buf, "+json");
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", strlen(jsonObject) + 1);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + strlen(jsonObject) + 1) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	strcat(buf, jsonObject);
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

	p1 = strstr(buf, "\"objectID\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memset(objectID, 0, KII_OBJECTID_SIZE + 1);
	memcpy(objectID, p1, p2 - p1);

	kiiHal_free(buf);
	return 0;
}

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
int kiiObj_createWithID(int scope, char* bucketName, char* jsonObject, char* dataType, char* objectID)
{
	return kiiObj_update(scope, bucketName, jsonObject, dataType, objectID, KIIOBJ_CREATE_WITH_ID);
}

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
int kiiObj_fullyUpdate(int scope, char* bucketName, char* jsonObject, char* dataType, char* objectID)
{
	return kiiObj_update(scope, bucketName, jsonObject, dataType, objectID, KIIOBJ_FULLY_UPDATE);
}

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
int kiiObj_partiallyUpdate(int scope, char* bucketName, char* jsonObject, char* objectID)
{
	return kiiObj_update(scope, bucketName, jsonObject, NULL, objectID, KIIOBJ_PARTIALLY_UPDATE);
}

/*****************************************************************************
*
*  kiiObj_fullyUpdate
*
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
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
static int
kiiObj_update(int scope, char* bucketName, char* jsonObject, char* dataType, char* objectID, int updateOrCreateWithID)
{
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	if(updateOrCreateWithID != KIIOBJ_PARTIALLY_UPDATE)
	{
		strcpy(buf, STR_PUT);
	}
	else
	{
		strcpy(buf, STR_POST);
	}
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
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
	if(updateOrCreateWithID == KIIOBJ_PARTIALLY_UPDATE)
	{
		// strcat(buf,  "If-Match:2");
		// strcat(buf, STR_CRLF);
		strcat(buf, "X-HTTP-Method-Override:PATCH");
		strcat(buf, STR_CRLF);
	}
	else
	{
		// content-type
		strcat(buf, STR_CONTENT_TYPE);
		strcat(buf, "application/vnd.");
		strcat(buf, g_kii_data.appID);
		strcat(buf, ".");
		strcat(buf, dataType);
		strcat(buf, "+json");
		strcat(buf, STR_CRLF);

		if(updateOrCreateWithID == KIIOBJ_FULLY_UPDATE)
		{
			// strcat(buf,  "If-Match:1");
			// strcat(buf, STR_CRLF);
		}
	}
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", strlen(jsonObject) + 1);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + strlen(jsonObject) + 1) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow!\r\n");
		kiiHal_free(buf);
		return -1;
	}
	strcat(buf, jsonObject);
	strcat(buf, STR_LF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if((strstr(buf, "HTTP/1.1 200") != NULL) || (strstr(buf, "HTTP/1.1 201") != NULL))
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
int kiiObj_uploadBodyAtOnce(int scope,
                            char* bucketName,
                            char* objectID,
                            char* dataType,
                            unsigned char* data,
                            unsigned int length)
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
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body");
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
	strcat(buf, dataType);
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", length);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + length) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	memcpy(buf + strlen(buf), data, length);
	// memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);
	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	if((strstr(buf, "HTTP/1.1 200")) != NULL)
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
int kiiObj_uploadBodyInit(int scope, char* bucketName, char* objectID, char* uploadID)
{
	char* p1;
	char* p2;
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
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body/uploads");
	strcat(buf, STR_HTTP);
	strcat(buf, STR_CRLF);
	// accept
	strcat(buf, STR_ACCEPT);
	strcat(buf, "application/vnd.kii.startobjectbodyuploadresponse+json");
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
	strcat(buf, "application/vnd.kii.startobjectbodyuploadrequest+json");
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", strlen(STR_EMPTY_JSON) + 1);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_EMPTY_JSON);
	strcat(buf, STR_LF);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}

	p1 = strstr(buf, "\"uploadID\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memset(uploadID, 0, KII_UPLOAD_ID_SIZE + 1);
	memcpy(uploadID, p1, p2 - p1);

	kiiHal_free(buf);
	return 0;
}

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
int kiiObj_uploadBody(int scope,
                      char* bucketName,
                      char* objectID,
                      char* uploadID,
                      char* dataType,
                      unsigned int position,
                      unsigned int length,
                      unsigned int totalLength,
                      unsigned char* data)
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
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body/uploads/");
	strcat(buf, uploadID);
	strcat(buf, "/data");
	strcat(buf, STR_HTTP);
	strcat(buf, STR_CRLF);
	// accept
	strcat(buf, STR_ACCEPT);
	strcat(buf, "application/json, application/*+json");
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
	strcat(buf, dataType);
	strcat(buf, STR_CRLF);
	// content-range
	strcat(buf, STR_CONTENT_RANGE);
	strcat(buf, "bytes=");
	sprintf(buf + strlen(buf), "%d", position);
	strcat(buf, "-");
	sprintf(buf + strlen(buf), "%d", position + length - 1);
	strcat(buf, "/");
	sprintf(buf + strlen(buf), "%d", totalLength);
	strcat(buf, STR_CRLF);
	// Authorization
	strcat(buf, STR_AUTHORIZATION);
	strcat(buf, " Bearer ");
	strcat(buf, g_kii_data.accessToken);
	strcat(buf, STR_CRLF);
	// Content-Length
	strcat(buf, STR_CONTENT_LENGTH);
	sprintf(buf + strlen(buf), "%d", length);
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if((strlen(buf) + length) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	memcpy(buf + strlen(buf), data, length);
	// memcpy(buf + g_kii_data.sendDataLen -1, STR_LF, 1);

	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if(strstr(buf, "HTTP/1.1 204") != NULL)
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
int kiiObj_uploadBodyCommit(int scope, char* bucketName, char* objectID, char* uploadID, int committed)
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
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body/uploads/");
	strcat(buf, uploadID);
	strcat(buf, "/status/");
	if(committed == 1)
	{
		strcat(buf, "committed");
	}
	else
	{
		strcat(buf, "cancelled");
	}
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

	if(strstr(buf, "HTTP/1.1 204") != NULL)
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
*  \param  scope - bucket scope
*               bucketName - the input of bucket name
*               objectID - the input of objectID
*               jsonObject - the output of object with json format
*               length - the buffer length of jsonObject
*
*  \return 0:success; -1: failure
*
*  \brief  Retrieves object with objectID
*
*****************************************************************************/
int kiiObj_retrieve(int scope, char* bucketName, char* objectID, char* jsonObject, unsigned int length)
{
	char* p1;
	char* buf;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_GET);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
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

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}

	p1 = strstr(buf, "{");
	if(strlen(p1) > length)
	{
		KII_DEBUG("kii-error: jsonObjectBuf overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}
	else
	{
		memset(jsonObject, 0, length);
		strcpy(jsonObject, p1);
		kiiHal_free(buf);
		return 0;
	}
}

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
int kiiObj_downloadBodyAtOnce(int scope,
                              char* bucketName,
                              char* objectID,
                              unsigned char* data,
                              unsigned int length,
                              unsigned int* actualLength)
{
	char* p1;
	char* buf;
	unsigned long contentLengh;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_GET);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body");
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
	// Accept
	strcat(buf, STR_ACCEPT);
	strcat(buf, "*/*");
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

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}

	p1 = strstr(buf, STR_CONTENT_LENGTH);
	if(p1 == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1 = p1 + strlen(STR_CONTENT_LENGTH);
	contentLengh = atoi(p1);

	p1 = strstr(buf, STR_CRLFCRLF);
	if(p1 == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1 += 4;

	if(contentLengh > length)
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
int kiiObj_downloadBody(int scope,
                        char* bucketName,
                        char* objectID,
                        unsigned int position,
                        unsigned int length,
                        unsigned char* data,
                        unsigned int* actualLength,
                        unsigned int* totalLength)
{
	char* p1;
	char* buf;
	unsigned long contentLengh;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		return -1;
	}
	memset(buf, 0, KII_SOCKET_BUF_SIZE);
	strcpy(buf, STR_GET);
	// url
	strcat(buf, "/api/apps/");
	strcat(buf, g_kii_data.appID);
	if(scope == KII_THING_SCOPE)
	{
		strcat(buf, "/things/VENDOR_THING_ID:");
		strcat(buf, g_kii_data.vendorDeviceID);
	}
	strcat(buf, "/buckets/");
	strcat(buf, bucketName);
	strcat(buf, "/objects/");
	strcat(buf, objectID);
	strcat(buf, "/body");
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
	// Accept
	strcat(buf, STR_ACCEPT);
	strcat(buf, "*/*");
	strcat(buf, STR_CRLF);
	// Range
	strcat(buf, STR_RANGE);
	strcat(buf, "bytes=");
	sprintf(buf + strlen(buf), "%d", position);
	strcat(buf, "-");
	sprintf(buf + strlen(buf), "%d", position + length - 1);
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

	if(strstr(buf, "HTTP/1.1 206") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}

	p1 = strstr(buf, STR_CONTENT_RANGE);
	p1 = strstr(p1, "/");
	if(p1 == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1++;
	*totalLength = atoi(p1);

	p1 = strstr(buf, STR_CONTENT_LENGTH);
	if(p1 == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1 = p1 + strlen(STR_CONTENT_LENGTH);
	contentLengh = atoi(p1);

	p1 = strstr(buf, STR_CRLFCRLF);
	if(p1 == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}
	p1 += 4;

	if(contentLengh > length)
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
