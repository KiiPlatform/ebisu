#include <string.h>
#include <stdio.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_hal.h"
#include "kii_device.h"

extern kii_data_struct g_kii_data;

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
int kiiDev_getToken(char* vendorDeviceID, char* password)
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
	strcat(buf, "/oauth2/token");
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
	strcat(buf, "application/vnd.kii.OauthTokenRequest+json");
	strcat(buf, STR_CRLF);

	memset(jsonBuf, 0, sizeof(jsonBuf));
	strcat(jsonBuf, "{\"username\":\"VENDOR_THING_ID:");
	strcat(jsonBuf, vendorDeviceID);
	strcat(jsonBuf, "\",\"password\":\"");
	strcat(jsonBuf, password);
	strcat(jsonBuf, "\",\"grant_type\": \"password\"}");

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

	if(strstr(buf, "HTTP/1.1 200") == NULL)
	{
		kiiHal_free(buf);
		return -1;
	}

	// get access token
	p1 = strstr(buf, "\"access_token\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memset(g_kii_data.accessToken, 0, KII_ACCESS_TOKEN_SIZE + 1);
	memcpy(g_kii_data.accessToken, p1, p2 - p1);

	memset(g_kii_data.vendorDeviceID, 0, KII_DEVICE_VENDOR_ID + 1);
	strcpy(g_kii_data.vendorDeviceID, vendorDeviceID);
	kiiHal_free(buf);
	return 0;
}

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
int kiiDev_register(char* vendorDeviceID, char* deviceType, char* password)
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
	strcat(buf, "/things/");
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
	strcat(buf, "application/vnd.kii.ThingRegistrationAndAuthorizationRequest+json");
	strcat(buf, STR_CRLF);

	memset(jsonBuf, 0, sizeof(jsonBuf));
	strcpy(jsonBuf, "{\"_vendorThingID\": \"");
	strcat(jsonBuf, vendorDeviceID);
	strcat(jsonBuf, "\",\"_thingType\": \"");
	strcat(jsonBuf, deviceType);
	strcat(jsonBuf, "\",\"_password\":\"");
	strcat(jsonBuf, password);
	strcat(jsonBuf, "\"}");

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

	p1 = strstr(buf, "\"_accessToken\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memset(g_kii_data.accessToken, 0, KII_ACCESS_TOKEN_SIZE + 1);
	memcpy(g_kii_data.accessToken, p1, p2 - p1);

	memset(g_kii_data.vendorDeviceID, 0, KII_DEVICE_VENDOR_ID + 1);
	strcpy(g_kii_data.vendorDeviceID, vendorDeviceID);

	kiiHal_free(buf);
	return 0;
}

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
int kiiDev_getIPAddress(char* ipAddress)
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
	strcpy(buf, STR_GET);
	// url
	strcat(buf, "/?format=json");
	strcat(buf, STR_HTTP);
	strcat(buf, STR_CRLF);
	// Host
	strcat(buf, "Host: ");
	strcat(buf, "api.ipify.org");
	strcat(buf, STR_CRLF);
	strcat(buf, STR_CRLF);
	if(strlen(buf) > KII_SOCKET_BUF_SIZE)
	{
		KII_DEBUG("kii-error: buffer overflow !\r\n");
		kiiHal_free(buf);
		return -1;
	}

	if(kiiHal_transfer("api.ipify.org", buf, KII_SOCKET_BUF_SIZE, strlen(buf)) != 0)
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

	p1 = strstr(buf, "\"ip\"");
	p1 = strstr(p1, ":");
	p1 = strstr(p1, "\"");
	p1 += 1;
	p2 = strstr(p1, "\"");
	memcpy(ipAddress, p1, p2 - p1);

	kiiHal_free(buf);
	return 0;
}
