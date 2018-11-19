#ifndef __example
#define __example

#ifdef __cplusplus
extern 'C' {
#endif

/* Go to https:/developer.kii.com and create app for you! */
const char KII_APP_ID[] = "fj9xy2fsp0ld";
/* JP: "api-jp.kii.com" */
/* US: "api.kii.com" */
/* SG: "api-sg.kii.com" */
/* CN: "api-cn3.kii.com" */
const char KII_APP_HOST[] = "api-jp.kii.com";

#define HANDLER_HTTP_BUFF_SIZE 4096
#define HANDLER_MQTT_BUFF_SIZE 2048
#define HANDLER_KEEP_ALIVE_SEC 300
#define UPDATER_HTTP_BUFF_SIZE 4096
#define UPDATE_PERIOD_SEC 60

#ifdef __cplusplus
}
#endif

#endif
