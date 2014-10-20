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

#define STR_BUCKET "myBucket"
#define STR_MEDIA_TYPE "myData"
#define STR_OBJECTBODY_TYPE "image/jpg"
#define STR_JSONOBJECT "{\"score\":\"1800\", \"name\":\"game1\"}"
#define STR_JSONOBJECT_PARTIALLY_UPDATE "{\"score\":\"1900\"}"
#define STR_JSONOBJECT_FULLY_UPDATE "{\"score\":\"1000\"}"
#define STR_OBJECT_ID "WinnerMicroTestObj"


extern int kiiUser_logIn(char *userName, char *password);


void kiiDemo_testObject(void)
{
    int ret;
    char objectID[KII_OBJECTID_SIZE+1];
    static unsigned char objectMultiplePiecesBody1[20];
    static unsigned char objectMultiplePiecesBody2[30];
    static unsigned char objectBody[20];

    printf("kii demo object test.\r\n");
    ret = kii_init(STR_SITE, STR_APPID, STR_APPKEY);
    if (ret < 0)
    {
    	    printf("kii init error\r\n");
	    return;
    }
	
    if (kiiUser_logIn("test1", "123456") != 0)
    {
	    return;
    }

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, NULL, objectID) < 0)
    {
        printf("kii create object without data type failed !\r\n");
        return;
    }
    else
    {
        printf("kii object is created without data type, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, STR_MEDIA_TYPE, objectID) < 0)
    {
        printf("kii create object with data type failed !\r\n");
        return;
    }
    else
    {
        printf("kii object is created with data type, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_createWithID(STR_BUCKET, STR_JSONOBJECT, NULL, STR_OBJECT_ID) < 0)
    {
        printf("kii object with ID and without data type failed !\r\n");
    }
    else
    {
        printf("kii object is created with ID and without data type , objectID:\"%s\"\r\n", STR_OBJECT_ID);
    }

    if (kiiObj_createWithID(STR_BUCKET, STR_JSONOBJECT, STR_MEDIA_TYPE, STR_OBJECT_ID) < 0)
    {
        printf("kii object with ID and data type failed !\r\n");
    }
    else
    {
        printf("kii object is created with ID and data type, objectID:\"%s\"\r\n", STR_OBJECT_ID);
    }

    if (kiiObj_partiallyUpdate(STR_BUCKET, STR_JSONOBJECT_PARTIALLY_UPDATE, objectID) < 0)
    {
        printf("kii object partially update failed !\r\n");
    }
    else
    {
        printf("kii object is partially updated, objectID:\"%s\"\r\n", objectID);
    }
	
    if (kiiObj_fullyUpdate(STR_BUCKET, STR_JSONOBJECT_FULLY_UPDATE, NULL,  objectID) < 0)
    {
        printf("kii object fully update without data type failed !\r\n");
    }
    else
    {
        printf("kii object is fully updated without data type, objectID:\"%s\"\r\n", objectID);
    }


    if (kiiObj_fullyUpdate(STR_BUCKET, STR_JSONOBJECT_FULLY_UPDATE, STR_MEDIA_TYPE,  objectID) < 0)
    {
        printf("kii object fully update with data type failed !\r\n");
    }
    else
    {
        printf("kii object is fully updated with data type, objectID:\"%s\"\r\n", objectID);
    }
	
    if (kiiObj_fullyUpdate(STR_BUCKET, STR_JSONOBJECT_FULLY_UPDATE, STR_MEDIA_TYPE,  objectID) < 0)
    {
        printf("kii object fully update with data type failed !\r\n");
    }
    else
    {
        printf("kii object is fully updated with data type, objectID:\"%s\"\r\n", objectID);
    }

    memset(objectBody, 'T', sizeof(objectBody));
    if (kiiObj_uploadBodyAtOnce(STR_BUCKET, objectID,  STR_OBJECTBODY_TYPE, objectBody, sizeof(objectBody)) < 0)
    {
        printf("kii object upload body at once  failed !\r\n");
    }
    else
    {
        printf("kii object body is uploaded at once, objectID:\"%s\"\r\n", objectID);
    }


    memset(objectMultiplePiecesBody1, 'U', sizeof(objectMultiplePiecesBody1));
    memset(objectMultiplePiecesBody2, 'E', sizeof(objectMultiplePiecesBody2));
    if (kiiObj_uploadBodyInit(STR_BUCKET, objectID, STR_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("kii object upload mutiple pieces body init failed !\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody1, sizeof(objectMultiplePiecesBody1)) < 0)
    {
        printf("kii object upload mutiple pieces of 1 failed !\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody2, sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("kii object upload mutiple pieces of 2 failed !\r\n");
    }
    else if (kiiObj_uploadBodyCommit(1) < 0)
    {
        printf("kii object upload multiple pieces body commit failed !\r\n");
    }
    else
    {
        printf("kii object upload multiple pieces body successfully !\r\n");
    }

    if (kiiObj_uploadBodyInit(STR_BUCKET, objectID, STR_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
    {
        printf("kii object upload mutiple pieces body init failed !\r\n");
    }
    else if (kiiObj_uploadBody(objectMultiplePiecesBody1, sizeof(objectMultiplePiecesBody1)) < 0)
    {
        printf("kii object upload mutiple pieces of 1 failed !\r\n");
    }
    else if (kiiObj_uploadBodyCommit(0) < 0)
    {
        printf("kii object upload multiple pieces body cancelled failed !\r\n");
    }
    else
    {
        printf("kii object upload multiple pieces body cancelled successfully !\r\n");
    }
}

void kiiDemo_pushMessageCallback(char* jsonBuf)
{
    printf("%s", jsonBuf);    
}


void kiiDemo_testPush(void)
{
    #define BUCKET_NAME  "LedControl"
    #define DEVICE_TYPE "Led"
    #define PASSWORD "123456"
	
    unsigned char mac_addr[8];
    char vendorID[KII_DEVICE_VENDOR_ID+1] ;
    int i;	
    int ret = 0;
    char objectID[KII_OBJECTID_SIZE+1];

    printf("kii demo push test.\r\n");
    ret = kii_init(STR_SITE, STR_APPID, STR_APPKEY);
    if (ret < 0)
    {
    	    printf("kii init error\r\n");
	    return;
    }

    memset(mac_addr,0,sizeof(mac_addr));
    tls_get_mac_addr(mac_addr);
    memset(vendorID, 0, sizeof(vendorID));
    for(i=0; i<6; i++)
    {
        sprintf(vendorID+strlen(vendorID), "%02x", mac_addr[i]);
    }
	
    //strcpy(vendorID+strlen(vendorID), "12");
	
    printf("verdorID:""%s""\r\n", vendorID);

	
    if (kiiDev_getToken(vendorID, PASSWORD) != 0)
    {
        printf("Get token failed, try to register it\r\n");
        ret = kiiDev_register(vendorID, DEVICE_TYPE, PASSWORD);
    }
	

    if (ret == 0)
    {
        printf("device activation success !\r\n");
    }
    else
    {
        printf("device activation error!\r\n");
	return;
    }

    if (kiiObj_create(BUCKET_NAME, STR_JSONOBJECT, NULL, objectID) < 0)
    {
	printf("kii create object without data type failed !\r\n");
	//return;
    }
    else
    {
	printf("kii object is created without data type, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiPush_subscribeBucket("AppLedControl") < 0)
    {
 	printf("Subscribe bucket error !\r\n");
	return;
    }
    else
    {
	printf("Subscribe bucket success !\r\n");
    }
	
    if (KiiPush_init(DEMO_KII_TASK_PRIO, kiiDemo_pushMessageCallback) < 0)
    {
	printf("Init push error !\r\n");
	return;
    }
    else
    {
	printf("Init push success !\r\n");
    }
}


int kiiDemo_test(char *buf)
{
    //kiiDemo_testObject();
    kiiDemo_testPush();
    return WM_SUCCESS;

}


