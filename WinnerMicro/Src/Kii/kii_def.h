#ifndef KII_DEF_H
#define KII_DEF_H

//#define KII_CLOUD_SUPPORT  1
#define KII_DEBUG_SUPPORT    1

#if KII_DEBUG_SUPPORT
#define KII_DEBUG printf
#else
#define KII_DEBUG if (0) printf
#endif



#define KII_SITE_SIZE 2
#define KII_HOST_SIZE 32
#define KII_APPID_SIZE 8
#define KII_APPKEY_SIZE 32
#define KII_NETBUF_SIZE 2048

#define KII_OBJECTID_SIZE 36


#define STR_POST "POST "
#define STR_PUT "PUT "
#define STR_HTTP "  HTTP/1.1"
#define STR_AUTHORIZATION "Authorization:"
#define STR_CONTENT_TYPE "content-type:"
#define STR_KII_APPID "x-kii-appid:"
#define STR_KII_APPKEY "x-kii-appkey:"
#define STR_CONTENT_LENGTH "Content-Length: "
#define STR_CRLF "\r\n"


#endif

