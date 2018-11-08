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

#define COMMAND_HANDLER_HTTP_BUFF_SIZE 4096
#define COMMAND_HANDLER_MQTT_BUFF_SIZE 2048
#define STATE_UPDATER_BUFF_SIZE 4096
#define STATE_UPDATE_PERIOD 60

#ifdef __cplusplus
}
#endif

#endif
