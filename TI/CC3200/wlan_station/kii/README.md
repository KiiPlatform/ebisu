# Desktop sample application on CC3200

## Set up options for target project
1. Copy ../../../../kii, ../../../../kii-core, ../../../../lib to current path, and copy kii_json.c and kii_json.h to ./kii
2. Add "DEBUG" defination in "Predefined Symbols" if debug mode is needed, and you can watch the traces in "Console" window supposing you are using CCS
3. Add related "include paths" in "Include Options" page.

## How to test
1. Set SSID_NAME, SECURITY_TYPE and SECURITY_KEY in the file of common.h 
2. If you want to test push notification with topic predefined, you need to comment out "kii_push_unsubscribe_topic" and "kii_push_delete_topic", then use the following commands to test it:
  curl -v -X POST \
  -H "Authorization: Bearer Mt0ZyBmGu31Wsc50ly2BJCadwoJl2RZTWgWXss1F4pY" \
  -H "content-type:application/vnd.kii.SendPushMessageRequest+json" \
  -H "x-kii-appid:9ab34d8b" \
  -H "x-kii-appkey:7a950d78956ed39f3b0815f0f001b43b" \
    "http://api-jp.kii.com/api/apps/9ab34d8b/things/VENDOR_THING_ID:4649/topics/my_topic/push/messages" \
  -d "{ \"data\" : {\"url\": \"https://dummy.com\", \"name\":\"game1\"}, \"mqtt\" : { \"enabled\": true}}"

## Issues
1. Struct "kii_socket_context_t" has no field "sl_Socket" issue:
We need to modify the defination of "kii_socket_context_t", use "int sock" feild instead of "int socket":

typedef struct kii_socket_context_t {
    void* app_context;
    int sock;
} kii_socket_context_t;

2. In file "kii_core.h", add the line "#define KII_SERVER_PORT 80" before the line "#ifndef KII_SERVER_PORT"
3. In file "kii_push.c", modify the follow lines:
  #define KIIPUSH_TASK_STK_SIZE 2048
  static unsigned int mKiiPush_taskStk[1];
  #if(KII_PUSH_PING_ENABLE)
  #define KIIPUSH_PINGREQ_TASK_STK_SIZE 1024
  static unsigned int mKiiPush_pingReqTaskStk[1];
  #endif
4. In "kii_json.h", add definition of "#define KII_JSON_FIXED_TOKEN_NUM 20"