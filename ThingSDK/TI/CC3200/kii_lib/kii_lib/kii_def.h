#ifndef KII_DEF_H
#define KII_DEF_H

#include "uart_if.h"
#include "common.h"

#include "kii.h"

#define KII_DEBUG_SUPPORT    1

#if KII_DEBUG_SUPPORT
#define KII_DEBUG UART_PRINT
#else
#define KII_DEBUG if (0) UART_PRINT
#endif


#ifndef NULL
#define NULL ((void *)0)
#endif


#define STR_POST "POST "
#define STR_PUT "PUT "
#define STR_GET "GET "
#define STR_HTTP " HTTP/1.1"
#define STR_AUTHORIZATION "Authorization: "
#define STR_CONTENT_TYPE "content-type: "
#define STR_KII_APPID "x-kii-appid: "
#define STR_KII_APPKEY "x-kii-appkey: "
#define STR_CONTENT_LENGTH "Content-Length: "
#define STR_ACCEPT "Accept: "
#define STR_RANGE "Range: "
#define STR_CONTENT_RANGE "Content-Range: "
#define STR_EMPTY_JSON "{ }"
#define STR_CRLF "\r\n"
#define STR_CRLFCRLF "\r\n\r\n"
#define STR_LF "\n"


#define KII_DEFAULT_PORT 80
#define KII_MQTT_DEFAULT_PORT 1883

typedef struct {
    char vendorDeviceID[KII_DEVICE_VENDOR_ID+1];
    char password[KII_PASSWORD_SIZE+1];
    char accessToken[KII_ACCESS_TOKEN_SIZE+1];
    char host[KII_HOST_SIZE+1];
    char appID[KII_APPID_SIZE+1];
    char appKey[KII_APPKEY_SIZE+1];
} kii_data_struct;



#endif

