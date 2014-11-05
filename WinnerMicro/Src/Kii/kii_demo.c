#include <string.h>
#include "wm_include.h"

#include "kii.h"

#if 0
#define STR_SITE "CN"
#define STR_APPID "41532a0f"
#define STR_APPKEY "72894abd7ab2f3fee1ec9814976e57c6"
#else
#define STR_SITE "JP"
#define STR_APPID "f25bd5bf"
#define STR_APPKEY "3594109968d7adf522c9991c0be51137"
#endif

#define STR_BUCKET "DevLedControl"
#define STR_JSONOBJECT_PARTIALLY_UPDATE "{\"Led\":\"On\"}"
#define STR_JSONOBJECT_FULLY_UPDATE "{\"Led\":\"On\"}"
#define STR_OBJECT_ID "WinnerMicroTestObj"

#define STR_BUCKET_FIRWAREUPGRADE "WM_FirmwareUpgrade"
#define STR_BUCKET_FROM_SERVER "LedControl"
#define STR_BUCKET_TO_SERVER "LedState"
#define STR_MEDIA_TYPE "Led"
#define STR_OBJECTBODY_TYPE "Firmware/bin"
#define STR_JSONOBJECT "{\"Led\":\"On\"}"
#define STR_DEVICE_TYPE "Led"
#define STR_PASSWORD "123456"
#define STR_THING_TOPIC   "WinnerMicro_Led"

char mVendorID[KII_DEVICE_VENDOR_ID+1] ;

void kiiDemo_testObject(void)
{
    char objectID[KII_OBJECTID_SIZE+1];
    char jsonObject[512];
    static unsigned char objectMultiplePiecesBody1[20];
    static unsigned char objectMultiplePiecesBody2[30];
    static unsigned char objectBody[512];
    int i;
    unsigned int bodyPosition;
    unsigned int bodyLength;
    unsigned int  bodyActualLength;
    unsigned int bodyTotalLength;
    unsigned char *bodyP;


    printf("kii demo object test.\r\n");

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, STR_MEDIA_TYPE, objectID) < 0)
    {
        printf("Create object failed\r\n");
    }
    else
    {
        printf("Create object success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_createWithID(STR_BUCKET, STR_JSONOBJECT, STR_MEDIA_TYPE, STR_OBJECT_ID) < 0)
    {
        printf("Create object with ID failed\r\n");
    }
    else
    {
        printf("Create object with ID success, objectID:\"%s\"\r\n", STR_OBJECT_ID);
    }

    if (kiiObj_partiallyUpdate(STR_BUCKET, STR_JSONOBJECT_PARTIALLY_UPDATE, objectID) < 0)
    {
        printf("Partially update object failed\r\n");
    }
    else
    {
        printf("Partially updated object success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_fullyUpdate(STR_BUCKET, STR_JSONOBJECT_FULLY_UPDATE, STR_MEDIA_TYPE,  objectID) < 0)
    {
        printf("Fully update object failed\r\n");
    }
    else
    {
        printf("Fully updated object success, objectID:\"%s\"\r\n", objectID);
    }
	
    memset(objectBody, 'T', sizeof(objectBody));
    if (kiiObj_uploadBodyAtOnce(STR_BUCKET, objectID,  STR_OBJECTBODY_TYPE, objectBody, sizeof(objectBody)) < 0)
    {
        printf("Upload object body at once failed\r\n");
    }
    else
    {
        printf("Uploaded object body at once success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_uploadBodyInit(STR_BUCKET, objectID, STR_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("Initialize uploading mutiple pieces object body failed\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody1, sizeof(objectMultiplePiecesBody1)) < 0)
    {
        printf("Upload mutiple pieces object body of part 1failed\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody2, sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("Upload mutiple pieces object body of part 2failed\r\n");
    }
    else if (kiiObj_uploadBodyCommit(0) < 0)
    {
        printf("Cancel uploadding multiple pieces object body failed\r\n");
    }
    else
    {
        printf("Upload then cancel  multiple pieces object body success\r\n");
    }

    for (i = 0; i< sizeof(objectMultiplePiecesBody1); i++)
    {
        objectMultiplePiecesBody1[i] = i;
    }
    for ( ; i< (sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)); i++)
    {
        objectMultiplePiecesBody1[i] = i;
    }
    if (kiiObj_uploadBodyInit(STR_BUCKET, objectID, STR_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("Initialize uploading mutiple pieces object body failed\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody1, sizeof(objectMultiplePiecesBody1)) < 0)
    {
        printf("Upload mutiple pieces object body of part 1failed\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody2, sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("Upload mutiple pieces object body of part 2failed\r\n");
    }
    else if (kiiObj_uploadBodyCommit(1) < 0)
    {
        printf("Commit uploadding multiple pieces object body failed\r\n");
    }
    else
    {
        printf("Upload and commit  multiple pieces object body success\r\n");
    }


    memset(jsonObject, 0, sizeof(jsonObject));
    if (kiiObj_retrieve(STR_BUCKET, objectID, jsonObject, sizeof(jsonObject)) < 0)
    {
        printf("Retrieve object failed, objectID:\"%s\"\r\n", objectID);
    }
    else
    {
	printf("Retrieve object  success, objectID:\"%s\"\r\njsonObject:\"%s\"\r\n", objectID, jsonObject);
    }

   printf("Testing downloading object body ...\r\n");
    memset(objectBody, 0, sizeof(objectBody));
    bodyPosition = 0;
    bodyLength = 5;
    bodyP = objectBody;
    if (kiiObj_downloadBody(STR_BUCKET, objectID,  bodyPosition,  bodyLength, bodyP, &bodyActualLength, &bodyTotalLength) < 0)
    {
        printf("Download object body failed, objectID:\"%s\"\r\n", objectID);
    }
    else
    {
	printf("objectID:\"%s\"\r\n", objectID);
	printf("bodyTotalLength:%d\r\n", bodyTotalLength);
        do {
		printf("bodyPosition:%d\r\n", bodyPosition);
		printf("bodyLength:%d\r\n", bodyLength);
		printf("bodyActualLength:%d\r\n", bodyActualLength);
		bodyPosition +=bodyActualLength;
		bodyP += bodyActualLength;
		if (kiiObj_downloadBody(STR_BUCKET, objectID,  bodyPosition,  bodyLength, bodyP, &bodyActualLength, &bodyTotalLength) < 0)
		{
			printf("Download object body failed, objectID:\"%s\"\r\n", objectID);
			bodyTotalLength = 0;
			break;
		}
        }while((bodyPosition + bodyActualLength) < bodyTotalLength);
	if (bodyTotalLength > 0)
	{
	    printf("Object body:\"");
	    for (i=0; i<bodyTotalLength; i++)
	    {
	        printf("%d ", objectBody[i]);
	    }
 	    printf("\"\r\n");
	}
    }
}

void kiiDemo_pushCallback(char* jsonBuf, int rcvdCounter)
{
    char * p1;
    char * p2;

    char objectID[KII_OBJECTID_SIZE+1];
    char bucketName[KII_BUCKET_NAME_SIZE+1];

    printf("Push callback: jsonbuf:\r\n%s\r\n", jsonBuf);
    p1 = strstr(jsonBuf, "objectID");
    if (p1 != NULL)
    {
        p1 +=8+3;
        p2 = strstr(p1, "\"");
	memset(objectID, 0 ,sizeof(objectID));
	memcpy(objectID, p1, p2-p1);
    }

    p1 = strstr(jsonBuf, "bucketID");
    if (p1 != NULL)
    {
        p1 +=8+3;
        p2 = strstr(p1, "\"");
	memset(bucketName, 0 ,sizeof(bucketName));
	memcpy(bucketName, p1, p2-p1);
    }

      printf("bucketID:%s\r\n", bucketName);
      printf("objectID:%s\r\n", objectID);

	if (strcmp(bucketName, STR_BUCKET_FIRWAREUPGRADE) == 0)
	{
	        printf("Firmware upgrade... !\r\n");
	}
	else if (strcmp(bucketName, STR_BUCKET_FROM_SERVER) == 0)
	{
	    if (kiiObj_createWithID(STR_BUCKET_TO_SERVER, STR_JSONOBJECT, STR_MEDIA_TYPE, mVendorID) < 0)
	    {
	        printf("Create object with ID failed\r\n");
	    }
	    else
	    {
	        printf("Create object with ID success\r\n");
	    }
	}
	else
	{
 	    printf("Invalid bucket ID\r\n");
	}

}


void kiiDemo_testPush(void)
{
    if (kiiPush_createTopic(STR_THING_TOPIC) < 0)
    {
 	printf("Create thing topic failed, topic:\"%s\"\r\n", STR_THING_TOPIC);
    }
    else
    {
	printf("Create thing topic success, topic:\"%s\"\r\n", STR_THING_TOPIC);
    }

    if (kiiPush_subscribeTopic(STR_THING_TOPIC) < 0)
    {
 	printf("Subscribe thing topic failed, topic:\"%s\"\r\n", STR_THING_TOPIC);
    }
    else
    {
	printf("Subscribe thing topic success, topic:\"%s\"\r\n", STR_THING_TOPIC);
    }

    if (kiiPush_subscribeAppBucket(STR_BUCKET_FIRWAREUPGRADE) < 0)
    {
	printf("Subscribe app bucket failed, bucket name::\"%s\"\r\n", STR_BUCKET_FIRWAREUPGRADE);
    }
    else
    {
	printf("Subscribe app bucket success, bucket name::\"%s\"\r\n", STR_BUCKET_FIRWAREUPGRADE);
    }

    if (kiiPush_subscribeThingBucket(STR_BUCKET_FROM_SERVER) < 0)
    {
	printf("Subscribe thing bucket failed, bucket name::\"%s\"\r\n", STR_BUCKET_FROM_SERVER);
    }
    else
    {
	printf("Subscribe thing bucket success, bucket name::\"%s\"\r\n", STR_BUCKET_FROM_SERVER);
    }

	
    if (KiiPush_init(DEMO_KII_PUSH_RECV_MSG_TASK_PRIO, DEMO_KII_PUSH_PINGREQ_TASK_PRIO, kiiDemo_pushCallback) < 0)
    {
	printf("Init push error !\r\n");
	return;
    }
    else
    {
	printf("Init push success !\r\n");
    }
}

int kiiDemo_OnBoarding(void)
{
    unsigned char mac_addr[8];
    int i;	

    printf("Device on boarding ...\r\n");
    memset(mac_addr,0,sizeof(mac_addr));
    tls_get_mac_addr(mac_addr);
    memset(mVendorID, 0, sizeof(mVendorID));
    for(i=0; i<6; i++)
    {
        sprintf(mVendorID+strlen(mVendorID), "%02x", mac_addr[i]);
    }
	
    //strcpy(mVendorID+strlen(mVendorID), "12");
	
    printf("verdorID:""%s""\r\n", mVendorID);

	
    if (kiiDev_getToken(mVendorID, STR_PASSWORD) != 0)
    {
        if (kiiDev_register(mVendorID, STR_DEVICE_TYPE, STR_PASSWORD) != 0)
        {
            printf("Onboarding failed\r\n");
	    return -1;
        }
	else
	{
            printf("Register thing success\r\n");
	    return 0;
	}
    }
    else
    {
        printf("Get thing token success\r\n");
	return 0;
    }
}

int kiiDemo_test(char *buf)
{
    if (kii_init(STR_SITE, STR_APPID, STR_APPKEY) < 0)
    {
       printf("Kii init failed\r\n");
       return WM_FAILED;
    }

    if (kiiDemo_OnBoarding() != 0)
    {
    	    printf("Device onbording failed\r\n");
	    return WM_FAILED;
    }

    kiiDemo_testObject();
    kiiDemo_testPush();
    return WM_SUCCESS;
}

