#ifndef LIGHT_H
#define LIGHT_H

//Thing onboarding
#define STR_DEVICE_TYPE "LED"
#define STR_PASSWORD "123456"

//remote control
#define STR_LED_BUCKET_CONTROL  "LEDControl"    //thing scope
#define STR_LED_BUCKET_RESPONSE  "LEDControlResponse"    //thing scope
#define STR_LED_MEDIA_TYPE "LED"

//remote control
#define STR_LED_BUCKET_FIRWAREUPGRADE "MTK_FirmwareUpgrade" //app scope

#define GPIO_LED1					13

#define STR_JSON_REQUESTID			"\"requestID\":"
#define STR_JSON_STATUS				"\"status\":"
#define STR_JSON_VALUE_TRUE		"true"
#define STR_JSON_VALUE_FALSE		"false"

#define STR_JSON_LIGHT_STATE		"\"state\":"
#define STR_JSON_LIGHT_COLOR		"\"color\":"
#define STR_JSON_LIGHT_BRIGHTNESS	"\"brightness\":"
#define STR_JSON_LIGHT_CT			"\"CT\":"

typedef struct {
	unsigned char state; //0,1
	unsigned char color [3]; //0..0xffffff
	unsigned char brightness; //0..100
	unsigned char ct; //0..100
} light_struct;



int light_initPush(void);
int light_onBoarding(void);


#endif

