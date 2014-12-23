#include <string.h>
#include <stdio.h>
#include<stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ctype.h>  


#include "kii.h"
#include "light.h"


/*****************************************************************************
*
*  light_control
*
*  \param  light - light struct
*
*  \return  0:success; -1: failure
*
*  \brief  Controls light
*
*****************************************************************************/
static int light_control(light_struct light)
{
   //TBD
    return 0;
}

/*****************************************************************************
*
*  kiiHal_dns
*
*  \param  *light - light struct poiter
*
*  \return  0:success; -1: failure
*
*  \brief  Gets light status
*
*****************************************************************************/
static int light_status(light_struct *light)
{
   //TBD
    return 0;
}


static int light_parseJson(char *jsonBuf, light_struct *light)
{
    char * p;
    unsigned long value;

    //state
    p = strstr(jsonBuf, STR_JSON_LIGHT_STATE);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_STATE);
        value = strtoul(p, 0 , 0);
	if (value <= 1)
	{
	    light->state = (unsigned char)value;
	}
	else
	{
	    return -1;
	}
    }

    // color
    p = strstr(jsonBuf, STR_JSON_LIGHT_COLOR);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_COLOR);
        value = strtoul(p, 0 , 0);
	if (value <= 0xffffff)
	{
	    light->color[2] = (unsigned char)(value >> 16);  //R
	    light->color[1] = (unsigned char)(value >> 8);  //G
	    light->color[0] = (unsigned char)value;  //B
	}
	else
	{
	    return -1;
	}
    }

    // brightness
    p = strstr(jsonBuf, STR_JSON_LIGHT_BRIGHTNESS);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_BRIGHTNESS);
        value = strtoul(p, 0 , 0);
	if (value <= 100)
	{
	    light->brightness = (unsigned char)value;
	}
	else
	{
	    return -1;
	}
    }

    // CT
    p = strstr(jsonBuf, STR_JSON_LIGHT_CT);
    if (p != NULL)
    {
        p +=strlen(STR_JSON_LIGHT_CT);
        value = strtoul(p, 0 , 0);
	if (value <= 100)
	{
	    light->ct= (unsigned char)value;
	}
	else
	{
	    return -1;
	}
    }
    return 0;
}

int light_buildJson(char *jsonBuf, light_struct light)
{
    //state
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_STATE);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.state);
    strcpy(jsonBuf+strlen(jsonBuf), ",");

    // color
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_COLOR);
    strcpy(jsonBuf+strlen(jsonBuf), "0x");
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[2]); //R
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[1]); //G
    sprintf(jsonBuf+strlen(jsonBuf), "%02x", light.color[0]); //B
    strcpy(jsonBuf+strlen(jsonBuf), ",");

    // brightness
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_BRIGHTNESS);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.brightness);
    strcpy(jsonBuf+strlen(jsonBuf), ",");
		
    // CT
    strcpy(jsonBuf+strlen(jsonBuf), STR_JSON_LIGHT_CT);
    sprintf(jsonBuf+strlen(jsonBuf), "%d", light.ct);

    return 0;
}

void light_callback(char* jsonBuf, int rcvdCounter)
{
    char * p1;
    char * p2;

    char objectID[KII_OBJECTID_SIZE+1];
    char bucketName[KII_BUCKET_NAME_SIZE+1];
    char jsonObject[512];
    light_struct light;
    int status;

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

    if (strcmp(bucketName, STR_LED_BUCKET_FIRWAREUPGRADE) == 0)
    {
        printf("Firmware upgrade... \r\n");
        //TODO: get url and and upgrade firmware
    }
    else if (strcmp(bucketName, STR_LED_BUCKET_CONTROL) == 0)
    {
        //printf("Remote control led... \r\n");
        //retrieve object
        memset(jsonObject, 0, sizeof(jsonObject));
        if (kiiObj_retrieve(STR_LED_BUCKET_CONTROL, objectID, jsonObject, sizeof(jsonObject)) < 0)
        {
            printf("Retrieve object failed, objectID:\"%s\"\r\n", objectID);
        }
        else
        {
            printf("jsonObject:%s\r\n", jsonObject);
            //printf("Retrieve object  success, objectID:\"%s\"\r\njsonObject:\"%s\"\r\n", objectID, jsonObject);
	    light_status(&light);
            if (light_parseJson(jsonObject, &light) == 0)
            {
                status = light_control(light);
		memset(jsonObject, 0, sizeof(jsonObject));
		strcpy(jsonObject, "{");
		light_buildJson(jsonObject+strlen(jsonObject), light);
		
		//add requestID field
		strcpy(jsonObject+strlen(jsonObject), ",");
		strcpy(jsonObject+strlen(jsonObject), STR_JSON_REQUESTID);
		strcpy(jsonObject+strlen(jsonObject), "\"");
		strcpy(jsonObject+strlen(jsonObject), objectID);
		strcpy(jsonObject+strlen(jsonObject), "\"");
		//add status field
		strcpy(jsonObject+strlen(jsonObject), ",");
		strcpy(jsonObject+strlen(jsonObject), STR_JSON_STATUS);
		if (status == 0)
		{
  		    strcpy(jsonObject+strlen(jsonObject), STR_JSON_VALUE_TRUE);
		}
	        else
	        {
 		    strcpy(jsonObject+strlen(jsonObject), STR_JSON_VALUE_FALSE);
	        }
		strcpy(jsonObject+strlen(jsonObject), "}");
		printf("light status:\r\n%s\r\n", jsonObject);
		if (kiiObj_create(STR_LED_BUCKET_RESPONSE, jsonObject, STR_LED_MEDIA_TYPE, objectID) < 0)
		{
		    printf("create led response object failed\r\n");
		}
		else
		{
		    printf("create led response success, objectID:%s\r\n", objectID);
		}
            }
            else
            {
                printf("Invalid Led command\r\n");
            }
        }
    }
    else
    {
        printf("Invalid bucket ID\r\n");
    }
}


int light_initPush(void)
{
    if (kiiPush_subscribeAppBucket(STR_LED_BUCKET_FIRWAREUPGRADE) < 0)
    {
	return -1;
    }

    if (kiiPush_subscribeThingBucket(STR_LED_BUCKET_CONTROL) < 0)
    {
	return -1;
    }
	
    if (KiiPush_init(0, 0, light_callback) < 0)
    {
	return -1;
    }
    else
    {
	return 0;
    }
}


static int light_getMacAddr(char *device, char *mac_addr)
{
    struct ifreq req;
    int socketNum;
    int err;
    int i;
	 
    socketNum = socket(AF_INET,SOCK_STREAM,0);
    if (socketNum < 0)
    {
        return -1;
    }
    strcpy(req.ifr_name,device);
    if (ioctl(socketNum,SIOCGIFHWADDR,&req) < 0)
    {
        close(socketNum);
        return -1;
    }
    else
    {
        close(socketNum);
	for(i=0; i<6; i++)
	{
		sprintf(mac_addr+strlen(mac_addr), "%02X", (unsigned	char)req.ifr_hwaddr.sa_data[i]);
	}
        return 0;    
    }
}

int light_onBoarding(void)
{
    char vendorID[KII_DEVICE_VENDOR_ID+1] ;

    memset(vendorID, 0, sizeof(vendorID));
//    if (light_getMacAddr("apcli0", vendorID) < 0)
    if (light_getMacAddr("eth0", vendorID) < 0)
    {
	printf("Get mac address failed\r\n");
        return -1;
    }
    printf("vendorID:""%s""\r\n", vendorID);

    if (kiiDev_getToken(vendorID, STR_PASSWORD) != 0)
    {
/*    
        if (kiiDev_register(vendorID, STR_DEVICE_TYPE, STR_PASSWORD) != 0)
        {
            printf("Onboarding failed\r\n");
	    return -1;
        }
	else
	{
            printf("Register thing success\r\n");
	    return 0;
	}
*/
        printf("Get thing token failed\r\n");
        return -1;
    }
    else
    {
        printf("Get thing token success\r\n");
	return 0;
    }
}


