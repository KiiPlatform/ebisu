#include <string.h>
#include "wm_include.h"

#include "kii.h"
//#include "kii_def.h"

#define STR_SITE "CN"
#define STR_APPID "41532a0f"
#define STR_APPKEY "72894abd7ab2f3fee1ec9814976e57c6"


#define STR_BUCKET "myBucket"
#define STR_MEDIA_TYPE "myData"
#define STR_OBJECTBODY_TYPE "image/jpg"
#define STR_JSONOBJECT "{\"score\":\"1800\", \"name\":\"game1\"}"
#define STR_JSONOBJECT_PARTIALLY_UPDATE "{\"score\":\"1900\"}"
#define STR_JSONOBJECT_FULLY_UPDATE "{\"score\":\"1000\"}"
#define STR_OBJECT_ID "WinnerMicroTestObj"


extern int kiiUser_logIn(char *userName, char *password);

int kiiDemo_test(char *buf)
{
    int ret;
    char objectID[KII_OBJECTID_SIZE+1];
    static unsigned char objectMultiplePiecesBody1[20];
    static unsigned char objectMultiplePiecesBody2[30];
    static unsigned char objectBody[20];
    
    printf("kii demo test.\r\n");
    ret = kii_init(STR_SITE, STR_APPID, STR_APPKEY);
    if (ret < 0)
    {
    	    printf("kii init error\r\n");
	    return WM_FAILED;
    }

    if (kiiUser_logIn("test1", "123456") != 0)
    {
	    return WM_FAILED;
    }

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, NULL, objectID) < 0)
    {
        printf("kii create object without data type failed !\r\n");
        return WM_FAILED;
    }
    else
    {
        printf("kii object is created without data type, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, STR_MEDIA_TYPE, objectID) < 0)
    {
        printf("kii create object with data type failed !\r\n");
        return WM_FAILED;
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

	
    return WM_SUCCESS;
}


