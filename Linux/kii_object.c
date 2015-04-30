#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"

#include "kii-core/kii.h"
#include "kii_core_impl.h"


int kiiObj_create(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_data,
        const char* object_content_type,
        char* object_id)
{
	char* p1;
	char* p2;
	char* buf;
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_create_new_object(
            kii,
            bucket,
            object_data,
            object_content_type);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

    buf = kii->http_context.buffer;
	p1 = strstr(buf, "\"objectID\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, ":");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, "\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 += 1;
	p2 = strstr(p1, "\"");
	if(p2 == NULL)
	{
		goto exit;
	}
	memcpy(object_id, p1, p2 - p1);
	ret = 0;

exit:
	return ret;
}

int kiiObj_createWithID(
	kii_t* kii,
	const kii_bucket_t* bucket,
	const char* object_id,
	const char* object_data,
	const char* object_content_type)
{
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_create_new_object_with_id(
            kii,
            bucket,
            object_id,
            object_data,
            object_content_type);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

	ret = 0;

exit:
	return ret;

}

int kiiObj_patch(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* patch_data,
        const char* opt_etag)
{
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_patch_object(
            kii,
            bucket,
            object_id,
            patch_data,
            opt_etag);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

	ret = 0;

exit:
	return ret;	
}

int kiiObj_replace(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        const char* replace_data,
        const char* opt_etag)
{
    int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_replace_object(
            kii,
            bucket,
            object_id,
            replace_data,
            opt_etag);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

	ret = 0;

exit:
	return ret;	
}

int kiiObj_get(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id,
        char* object_data,
        size_t size)
{
	char* p1;
	char* buf;
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_get_object(
            kii,
            bucket,
            object_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

    buf = kii->http_context.buffer;
	p1 = strstr(buf, "{");
	if(p1 == NULL)
	{
		goto exit;
	}
	if(strlen(p1) >= size)
	{
		M_KII_LOG(kii->logger_cb("Object buffer overflow!\n"));
		goto exit;
	}
	else
	{
		memset(object_data, 0, size);
		strcpy(object_data, p1);
		ret = 0;
	}
exit:
	return ret;
}

int kiiObj_delete(
        kii_t* kii,
        const kii_bucket_t* bucket,
        const char* object_id)
{
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    core_err = kii_delete_object(
            kii,
            bucket,
            object_id);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
    if(kii->response_code < 200 || 300 <= kii->response_code)
    {
		goto exit;
    }

	ret = 0;
exit:
	return ret;
}

#if 0
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}
	memcpy(buf + strlen(buf), data, length);
	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		goto free;
	}
	if((strstr(buf, "HTTP/1.1 200")) != NULL)
	{
		ret = 0;
	}
	
free:
	kiiHal_free(buf);
exit:
	return ret;
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		goto free;
	}

	p1 = strstr(buf, "\"uploadID\"");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = strstr(p1, ":");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = strstr(p1, "\"");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 += 1;
	p2 = strstr(p1, "\"");
	if(p2 == NULL)
	{
		goto free;
	}
	memset(uploadID, 0, KII_UPLOAD_ID_SIZE + 1);
	memcpy(uploadID, p1, p2 - p1);
	ret = 0;
	
free:
	kiiHal_free(buf);
exit:
	return ret;
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}
	memcpy(buf + strlen(buf), data, length);
	if(kiiHal_transfer(g_kii_data.host, buf, KII_SOCKET_BUF_SIZE, strlen(buf) + length) != 0)
	{
		KII_DEBUG("kii-error: transfer data error !\r\n");
		goto free;
	}

	if(strstr(buf, "HTTP/1.1 204") != NULL)
	{
		ret = 0;
	}
	
free:
	kiiHal_free(buf);
exit:
	return ret;
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}

	if(strstr(buf, "HTTP/1.1 204") != NULL)
	{
		ret = 0;
	}

free:
	kiiHal_free(buf);
exit:
	return ret;
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		goto free;
	}

	p1 = strstr(buf, STR_CONTENT_LENGTH);
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = p1 + strlen(STR_CONTENT_LENGTH);
	contentLengh = atoi(p1);

	p1 = strstr(buf, STR_CRLFCRLF);
	if(p1 == NULL)
	{
		goto free;
	}
	p1 += 4;

	if(contentLengh > length)
	{
		KII_DEBUG("kii-error: the buffer of object body is too small\r\n");
		goto free;
	}
	else
	{
		memset(data, 0, length);
		memcpy(data, p1, contentLengh);
		*actualLength = contentLengh;
		ret = 0;
	}
	
free:
	kiiHal_free(buf);
exit:
	return ret;
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
	int ret = -1;

	buf = kiiHal_malloc(KII_SOCKET_BUF_SIZE);
	if(buf == NULL)
	{
		KII_DEBUG("kii-error: memory allocation failed !\r\n");
		goto exit;
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
		goto free;
	}

	if(strstr(buf, "HTTP/1.1 206") == NULL)
	{
		goto free;
	}

	p1 = strstr(buf, STR_CONTENT_RANGE);
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = strstr(p1, "/");
	if(p1 == NULL)
	{
		goto free;
	}
	p1++;
	*totalLength = atoi(p1);

	p1 = strstr(buf, STR_CONTENT_LENGTH);
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = p1 + strlen(STR_CONTENT_LENGTH);
	contentLengh = atoi(p1);

	p1 = strstr(buf, STR_CRLFCRLF);
	if(p1 == NULL)
	{
		goto free;
	}
	p1 += 4;

	if(contentLengh > length)
	{
		KII_DEBUG("kii-error: the buffer of object body is too small");
		goto free;
	}
	else
	{
		memset(data, 0, length);
		memcpy(data, p1, contentLengh);
		*actualLength = contentLengh;
		ret = 0;
	}
	
free:
	kiiHal_free(buf);
exit:
	return ret;
}
#endif
