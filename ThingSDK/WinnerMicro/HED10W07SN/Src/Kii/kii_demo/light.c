#include <string.h>
#include "wm_include.h"

#include "kii.h"
#include "light.h"
#include "light_if.h"


static char mVendorID[KII_DEVICE_VENDOR_ID+1] ;
static char mModelName[LIGHT_MODEL_NAME_SIZE+1];


static int light_parseJson(char *jsonBuf, light_struct *light)
{
    char * p;
    unsigned long value;

    memset(light, 0, sizeof(light_struct));
    //state
    p = strstr(jsonBuf, STR_JSON_LIGHT_STATE);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_STATE);
        value = strtoul(p, 0 , 0);
        light->state = (unsigned char)value;
	light->effectiveField |= LIGHT_EFFECTIVE_FIELD_STATE;
    }

    // color, ignore alpha value if exist
    p = strstr(jsonBuf, STR_JSON_LIGHT_COLOR);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_COLOR);
        value = strtoul(p, 0 , 0);
        light->color[0] = (unsigned char)(value >> 16);  //R
        light->color[1] = (unsigned char)(value >> 8);  //G
        light->color[2] = (unsigned char)value;  //B
	light->effectiveField |= LIGHT_EFFECTIVE_FIELD_COLOR;
    }

    // brightness
    p = strstr(jsonBuf, STR_JSON_LIGHT_BRIGHTNESS);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_BRIGHTNESS);
        value = strtoul(p, 0 , 0);
        light->brightness = (unsigned char)value;
	light->effectiveField |= LIGHT_EFFECTIVE_FIELD_BRIGHTESS;
    }

    // CT
    p = strstr(jsonBuf, STR_JSON_LIGHT_CT);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_CT);
        value = strtoul(p, 0 , 0);
        light->ct= (unsigned char)value;
	light->effectiveField |= LIGHT_EFFECTIVE_FIELD_CT;
    }

    //mode	
    p = strstr(jsonBuf, STR_JSON_LIGHT_MODE);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_MODE);
        value = strtoul(p, 0 , 0);
	if (value == LIGHT_MODE_COLOR_ACTIVE)
	{
	    light->effectiveField &=(~ LIGHT_EFFECTIVE_FIELD_CT);
	}
	else if (value == LIGHT_MODE_CT_ACTIVE)
	{
	    light->effectiveField &=(~ LIGHT_EFFECTIVE_FIELD_COLOR);
	}
    }
    return 0;
}

static int light_buildJson(char *jsonBuf, light_struct light)
{
    //state
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_STATE);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.state);
    strcpy(jsonBuf+strlen(jsonBuf), ",");

    // color
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_COLOR);
    strcpy(jsonBuf+strlen(jsonBuf), "0x");
    strcpy(jsonBuf+strlen(jsonBuf), "ff"); //alpha
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[0]); //R
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[1]); //G
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[2]); //B
    strcpy(jsonBuf+strlen(jsonBuf), ",");

    // brightness
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_BRIGHTNESS);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.brightness);
    strcpy(jsonBuf+strlen(jsonBuf), ",");
		
    // CT
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_CT);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.ct);
    strcpy(jsonBuf+strlen(jsonBuf), ",");

    // mode
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_MODE);
    if ((light.effectiveField & LIGHT_EFFECTIVE_FIELD_COLOR) > 0)
    {
	sprintf(jsonBuf+strlen(jsonBuf), "%d", LIGHT_MODE_COLOR_ACTIVE);
    }
    else if((light.effectiveField & LIGHT_EFFECTIVE_FIELD_CT) > 0)
    {
	sprintf(jsonBuf+strlen(jsonBuf), "%d", LIGHT_MODE_CT_ACTIVE);
    }

    return 0;
}


int light_updateStatus(light_struct light)
{
    char jsonObject[512];

    memset(jsonObject, 0, sizeof(jsonObject));
    strcpy(jsonObject, "{");
    light_buildJson(jsonObject+strlen(jsonObject), light);
    strcpy(jsonObject+strlen(jsonObject), ",");
    strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    strcpy(jsonObject+strlen(jsonObject), mVendorID);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    strcpy(jsonObject+strlen(jsonObject), "}");
	
    printf("action response json:\r\n%s\r\n", jsonObject);
    if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
    {
        printf("do action response failed\r\n");
	return -1;
    }
    else
    {
        return 0;
    }
}

void light_parseLedControl(char *bucketName, char *objectID)
{
    char jsonObject[LIGHT_JSON_OBJECT_SIZE+1];
    char type[LIGHT_TYPE_SIZE+1];
    char pwd[LIGHT_PASSWORD_SIZE+1];
    char *p1;
    char *p2;	
    light_struct light;

    //retrieve object
    memset(jsonObject, 0, sizeof(jsonObject));
    if (kiiObj_retrieve(KII_THING_SCOPE, bucketName, objectID, jsonObject, sizeof(jsonObject)) < 0)
    {
        printf("Retrieve object failed, objectID:\"%s\"\r\n", objectID);
        return;
    }
	
    //get type
    memset(type, 0 ,sizeof(type));
    p1 = strstr(jsonObject, STR_JSON_TYPE);
    if (p1 != NULL)
    {
        p1 +=strlen(STR_JSON_TYPE);
        p1 = strstr(p1, "\"");
        p1++;
        p2 = strstr(p1, "\"");
        memcpy(type, p1, p2-p1);
    }
    else
    {
        printf("get type failed\r\n");
        return;
    }

    if (strcmp(type, STR_JSON_TYPE_COMMAND) == 0)
    {
        if (light_parseJson(jsonObject, &light) == 0)
        {
            lightIf_control(light);
 	    lightIf_status(&light); //get light whole status
            memset(jsonObject, 0, sizeof(jsonObject));
            strcpy(jsonObject, "{");
            light_buildJson(jsonObject+strlen(jsonObject), light);
            //add requestID field
            strcpy(jsonObject+strlen(jsonObject), ",");
            strcpy(jsonObject+strlen(jsonObject), STR_JSON_REQUESTID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), objectID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            // add thingID field
            strcpy(jsonObject+strlen(jsonObject), ",");
            strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), mVendorID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), "}");
            printf("action response json:\r\n%s\r\n", jsonObject);
            if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
            {
                printf("do action response failed\r\n");
            }
            else
            {
                //printf("do action response success\r\n");
            }
        }
        else
        {
            printf("Invalid light control command\r\n");
        }
    }
    else if (strcmp(type, STR_JSON_TYPE_QUERYSTATUS) == 0)
    {
        lightIf_status(&light);
        memset(jsonObject, 0, sizeof(jsonObject));
        strcpy(jsonObject, "{");
        light_buildJson(jsonObject+strlen(jsonObject), light);
        //add requestID field
        strcpy(jsonObject+strlen(jsonObject), ",");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_REQUESTID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), objectID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        // add thingID field
        strcpy(jsonObject+strlen(jsonObject), ",");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), mVendorID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), "}");
        printf("action response json:\r\n%s\r\n", jsonObject);
        if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
        {
            printf("do action response failed\r\n");
        }
        else
        {
            //printf("do action response success\r\n");
        }
    }
    else if (strcmp(type, STR_JSON_TYPE_UPDATEPWD) == 0)
    {
        //get password
        memset(pwd, 0 ,sizeof(pwd));
        p1 = strstr(jsonObject, STR_JSON_NEW_PWD);
        if (p1 != NULL)
        {
            p1 +=strlen(STR_JSON_NEW_PWD);
            p1 = strstr(p1, "\"");
            p1++;
            p2 = strstr(p1, "\"");
            memcpy(pwd, p1, p2-p1);
            lightIf_updatePassword(pwd);
            //response to cloud
            memset(jsonObject, 0, sizeof(jsonObject));
            strcpy(jsonObject, "{");
            //add adminPwd field
            strcpy(jsonObject+strlen(jsonObject), STR_JSON_PASSWORD);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), pwd);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            //add requestID field
            strcpy(jsonObject+strlen(jsonObject), ",");
            strcpy(jsonObject+strlen(jsonObject), STR_JSON_REQUESTID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), objectID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            // add thingID field
            strcpy(jsonObject+strlen(jsonObject), ",");
            strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), mVendorID);
            strcpy(jsonObject+strlen(jsonObject), "\"");
            strcpy(jsonObject+strlen(jsonObject), "}");
            printf("action response json:\r\n%s\r\n", jsonObject);
            if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
            {
                printf("do action response failed\r\n");
            }
            else
            {
                //printf("do action response success\r\n");
            }
        }
        else
        {
            printf("get password failed\r\n");
        }
    }
    else if (strcmp(type, STR_JSON_TYPE_FACTORY_RESET) == 0)
    {
        memset(jsonObject, 0, sizeof(jsonObject));
        strcpy(jsonObject, "{");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_FACTORY_RESET);
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_TRUE);
        //add requestID field
        strcpy(jsonObject+strlen(jsonObject), ",");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_REQUESTID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), objectID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        // add thingID field
        strcpy(jsonObject+strlen(jsonObject), ",");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), mVendorID);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), "}");
        printf("action response json:\r\n%s\r\n", jsonObject);
        if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
        {
            printf("do action response failed\r\n");
        }
        else
        {
            printf("factory reset...\r\n");
            lightIf_factoryReset();
        }
    }
    else
    {
        printf("invalid type:""%s""\r\n", type);
    }
}



void light_parseFirmwareUpgrade(char *bucketName, char *objectID)
{
    char jsonObject[LIGHT_JSON_OBJECT_SIZE+1];
    //char pointerObjectID[KII_OBJECTID_SIZE+1];
    //char pointerBucketName[KII_BUCKET_NAME_SIZE+1];
    char bodyUrl[LIGHT_FIRMWARE_UPGRADE_URL_SIZE+1];
    char versionName[LIGHT_VERSION_NAME_SIZE+1];
    char *p1;
    char *p2;	


    //retrieve object
    memset(jsonObject, 0, sizeof(jsonObject));
    if (kiiObj_retrieve(KII_APP_SCOPE, bucketName, objectID, jsonObject, sizeof(jsonObject)) < 0)
    {
        printf("Retrieve object failed, objectID:\"%s\"\r\n", objectID);
        return;
    }
    printf("jsonObject:%s\r\n", jsonObject);
    //get  version name
    memset(versionName, 0 ,sizeof(versionName));
    p1 = strstr(jsonObject, STR_JSON_VERSION_NAME);
    if (p1 != NULL)
    {
        p1 +=strlen(STR_JSON_VERSION_NAME);
        p1 = strstr(p1, "\"");
        p1++;
        p2 = strstr(p1, "\"");
        memcpy(versionName, p1, p2-p1);
    }
    else
    {
        printf("get version name failed\r\n");
        return;
    }

    //get body url
    memset(bodyUrl, 0 ,sizeof(bodyUrl));
    p1 = strstr(jsonObject, STR_JSON_FIRMWARE_URL);
    if (p1 != NULL)
    {
        p1 +=strlen(STR_JSON_FIRMWARE_URL);
        p1 = strstr(p1, "\"");
        p1++;
        p2 = strstr(p1, "\"");
        memcpy(bodyUrl, p1, p2-p1);
    }
    else
    {
        printf("get body url failed\r\n");
        return;
    }
    printf("firmware upgrade ...\r\n");
    printf("bodyUrl:%s\r\n", bodyUrl);
    printf("versionName:%s\r\n", versionName);
    lightIf_firmwareUpgrade(bodyUrl, versionName);
}

void light_callback(char* jsonBuf, int rcvdCounter)
{
    char objectID[KII_OBJECTID_SIZE+1];
    char bucketName[KII_BUCKET_NAME_SIZE+1];
    char * p1;
    char * p2;

    //printf("Push callback: jsonbuf:\r\n%s\r\n", jsonBuf);
    p1 = strstr(jsonBuf, "\"objectID\":\"");
    if (p1 != NULL)
    {
        p1 +=12;
        p2 = strstr(p1, "\"");
        memset(objectID, 0 ,sizeof(objectID));
        memcpy(objectID, p1, p2-p1);
    }
    else
    {
        printf("get objectID failed\r\n");
        return;
    }

    p1 = strstr(jsonBuf, "\"bucketID\":\"");
    if (p1 != NULL)
    {
        p1 +=12;
        p2 = strstr(p1, "\"");
        memset(bucketName, 0 ,sizeof(bucketName));
        memcpy(bucketName, p1, p2-p1);
    }
    else
    {
        printf("get bucketID failed\r\n");
        return;
    }

    //printf("bucketID:%s\r\n", bucketName);
    //printf("objectID:%s\r\n", objectID);
    if (strcmp(bucketName, STR_LED_BUCKET_CONTROL) == 0)
    {
        light_parseLedControl(bucketName, objectID);
    }
    else
    {
	if (strcmp(bucketName, mModelName) == 0)
	{
	    light_parseFirmwareUpgrade(bucketName, objectID);
	}
	else
	{
            printf("Invalid bucket name\r\n");
	}
    }
}


int light_initPush(void)
{
    if (kiiPush_subscribeBucket(KII_APP_SCOPE, mModelName) < 0)
    {
        return -1;
    }
    if (kiiPush_subscribeBucket(KII_THING_SCOPE, STR_LED_BUCKET_CONTROL) < 0)
    {
	return -1;
    }

    if (KiiPush_init(DEMO_KII_PUSH_RECV_MSG_TASK_PRIO, DEMO_KII_PUSH_PINGREQ_TASK_PRIO, light_callback) < 0)
    {
	return -1;
    }
    else
    {
	return 0;
    }
}


int light_onBoarding(void)
{
    if (kiiDev_getToken(mVendorID, STR_PASSWORD) != 0)
    {
        printf("Get thing token failed\r\n");
        return -1;
    }
    else
    {
        printf("Get thing token success\r\n");
	return 0;
    }
}

int light_updateBootupStatus(void)
{
    char jsonObject[LIGHT_JSON_OBJECT_SIZE+1];
    char pwd[LIGHT_PASSWORD_SIZE+1];
    char firmwareVersion[LIGHT_VERSION_NAME_SIZE+1];
    char ipAddress[LIGHT_IP_ADDRESS_SIZE+1];
    light_struct light;

    memset(jsonObject, 0, sizeof(jsonObject));
    // add light status
    lightIf_status(&light);
    strcpy(jsonObject, "{");
    light_buildJson(jsonObject+strlen(jsonObject), light);
    //add firmware version field
    strcpy(jsonObject+strlen(jsonObject), ",");
    strcpy(jsonObject+strlen(jsonObject), STR_JSON_FIRMWARE_VERSION);
    memset(firmwareVersion, 0, sizeof(firmwareVersion));
    lightIf_getFirmwareVersion(firmwareVersion);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    strcpy(jsonObject+strlen(jsonObject), firmwareVersion);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    //add password field after factory reset
    memset(pwd, 0, sizeof(pwd));
    lightIf_getPassword(pwd);
    if (strlen(pwd) > 0)
    {
        strcpy(jsonObject+strlen(jsonObject), ",");
        strcpy(jsonObject+strlen(jsonObject), STR_JSON_PASSWORD);
        strcpy(jsonObject+strlen(jsonObject), "\"");
        strcpy(jsonObject+strlen(jsonObject), pwd);
        strcpy(jsonObject+strlen(jsonObject), "\"");
    }
    //add ip address field
    strcpy(jsonObject+strlen(jsonObject), ",");
    strcpy(jsonObject+strlen(jsonObject), STR_JSON_IP_ADDRESS);
    memset(ipAddress, 0, sizeof(ipAddress));
    lightIf_getIPAddress(ipAddress);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    strcpy(jsonObject+strlen(jsonObject), ipAddress);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    // add thingID field
    strcpy(jsonObject+strlen(jsonObject), ",");
    strcpy(jsonObject+strlen(jsonObject), STR_JSON_THINGID);
    strcpy(jsonObject+strlen(jsonObject), "\"");
    strcpy(jsonObject+strlen(jsonObject), mVendorID);
    strcpy(jsonObject+strlen(jsonObject), "\"");
		
    strcpy(jsonObject+strlen(jsonObject), "}");
    printf("action response json:\r\n%s\r\n", jsonObject);
    if (kiiExt_extension(STR_EXTENSION_DO_ACTION_RESPONSE, jsonObject) < 0)
    {
        printf("do action response failed\r\n");
        return -1;
    }
    else
    {
	//printf("do action response success\r\n");
        if (strlen(pwd) > 0)
        {
            memset(pwd, 0, sizeof(pwd));
            lightIf_updatePassword(pwd);
        }
	return 0;
    }
}

int light_init(void)
{
    memset(mVendorID, 0, sizeof(mVendorID));
    if (lightIf_getMacAddr(mVendorID) < 0)
    {
	printf("Get mac address failed\r\n");
        return -1;
    }
    printf("vendorID:%s\r\n", mVendorID);

    memset(mModelName, 0, sizeof(mModelName));
    if (lightIf_getIModelName(mModelName) < 0)
    {
	printf("Get model name failed\r\n");
	return -1;
    }
    printf("Model name:%s\r\n", mModelName);

    if (kii_init(STR_SITE, STR_APPID, STR_APPKEY) < 0)
    {
       return -1;
    }

    if (light_onBoarding() != 0)
    {
    	    printf("Light onbording failed\r\n");
	    return -1;
    }
    else
    {
	printf("light onboarding success\r\n");
    }

    if (light_initPush() < 0)
    {
        printf("Initialize push failed\r\n");
	return -1;
    }
    else
    {
	printf("Initialize push success\r\n");
    }
	
    if (light_updateBootupStatus() < 0)
    {
	printf("Update bootup status failed\r\n");
	return -1;
    }
    else
    {
	printf("Update bootup status success\r\n");
    }
	
    return 0;
}
