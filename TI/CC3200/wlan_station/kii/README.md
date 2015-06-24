# Desktop sample application on CC3200

## Set up options for target project
1. Copy ../../../../kii, ../../../../kii-core, ../../../../lib to current path
1. Add "DEBUG" defination in "Predefined Symbols" if debug mode is needed, and you can watch the traces in "Console" window supposing you are using CCS
2. Add related "include paths" in "Include Options" page.

## How to test
1. Set SSID_NAME, SECURITY_TYPE and SECURITY_KEY in the file of common.h 
2. if you want to test push notification with topic predefined, you need to comment out "kii_push_unsubscribe_topic" and "kii_push_delete_topic"

## Issues
1. Need to modify "prv_kii_util_get_http_body" in file "kii_util.c":

char* prv_kii_util_get_http_body(char* str, size_t len)
{
    char* ret = NULL;

    ret = strstr(str, "\r\n\r\n");
    if (ret != NULL) {
        ret += 4;
    }

    return ret;
}

2. Struct "kii_socket_context_t" has no field "sl_Socket" issue:
We need to modify the defination of "kii_socket_context_t", use "int sock" feild instead of "int socket":

typedef struct kii_socket_context_t {
    void* app_context;
    int sock;
} kii_socket_context_t;

3. Creating new object with api "kii_object_create" is unstable
