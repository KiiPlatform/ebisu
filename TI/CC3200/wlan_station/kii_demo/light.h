#ifndef LIGHT_H
#define LIGHT_H

#include "light_if.h"

#define STR_SITE "JP"
#define STR_APPID "06e806e2"
#define STR_APPKEY "31afdcdfd72ade025559176a40a20875"

// server extension
#define STR_EXTENSION_DO_ACTION_RESPONSE "doActionResponse"

// Thing onboarding
#define STR_DEVICE_TYPE "LED"
#define STR_PASSWORD "123456"

#define LIGHT_TYPE_SIZE 16
#define LIGHT_JSON_OBJECT_SIZE 512

// remote control
#define STR_LED_BUCKET_CONTROL "LEDControl"          // thing scope
#define STR_LED_BUCKET_RESPONSE "LEDControlResponse" // thing scope
#define STR_LED_MEDIA_TYPE "LED"

// firmware upgrade
#define STR_JSON_TYPE_FIRMWAREUPGRADE "firmwareUpgrade"
#define STR_JSON_FIRMWARE_URL "\"firmwareUrl\":"
#define STR_JSON_VERSION_NAME "\"versionName\":"
#define STR_JSON_FIRMWARE_BUCKET_PREFIX "FirmwareUpgrade_"

#define STR_JSON_TYPE "\"type\":"

#define STR_JSON_TYPE_COMMAND "command"
#define STR_JSON_LIGHT_STATE "\"state\":"
#define STR_JSON_LIGHT_COLOR "\"color\":"
#define STR_JSON_LIGHT_BRIGHTNESS "\"brightness\":"
#define STR_JSON_LIGHT_CT "\"CT\":"
#define STR_JSON_LIGHT_MODE "\"mode\":"
#define LIGHT_MODE_COLOR_ACTIVE 0
#define LIGHT_MODE_CT_ACTIVE 1

#define STR_JSON_TYPE_QUERYSTATUS "queryStatus"

#define STR_JSON_TYPE_UPDATEPWD "updatePwd"
#define STR_JSON_NEW_PWD "\"newPwd\":"
#define STR_JSON_PASSWORD "\"password\":"

#define STR_JSON_TYPE_FACTORY_RESET "factoryReset"
#define STR_JSON_FACTORY_RESET "\"factoryReset\":"

#define STR_JSON_REQUESTID "\"requestID\":"
#define STR_JSON_THINGID "\"thingID\":"

#define STR_JSON_FIRMWARE_VERSION "\"firmwareVersion\":"

#define STR_JSON_IP_ADDRESS "\"ipAddress\":"

#define STR_JSON_TRUE "true"
#define STR_JSON_FALSE "false"

int light_init(void);
int light_updateStatus(light_struct light);

#endif
