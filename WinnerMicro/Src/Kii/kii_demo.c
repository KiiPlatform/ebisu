#include <string.h>
#include "wm_include.h"

#include "kii.h"
#include "kii_def.h"

#define STR_SITE "CN"
#define STR_APPID "41532a0f"
#define STR_APPKEY "72894abd7ab2f3fee1ec9814976e57c6"


#define STR_BUCKET "myBucket"
#define STR_JSONOBJECT "{\"score\":\"1800\", \"name\":\"game1\"}\n"
#define STR_JSONOBJECT_PARTIALLY_UPDATE "{\"score\":\"1900\"}\n"
#define STR_JSONOBJECT_FULLY_UPDATE "{\"score\":\"1000\"}\n"
#define STR_OBJECT_ID "WinnerMicroTestObj"


extern int kiiUser_logIn(char *userName, char *password);

int kiiDemo_test(char *buf)
{
    int ret;
    char objectID[KII_OBJECTID_SIZE+1];
    
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

    if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, objectID) < 0)
    {
        printf("kii create object error!\r\n");
        return WM_FAILED;
    }
    else
    {
        printf("kii object is created, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_createWithID(STR_BUCKET, STR_JSONOBJECT, STR_OBJECT_ID) < 0)
    {
        printf("kii object fully update error!\r\n");
        //return WM_FAILED;
    }
    else
    {
        printf("kii object is fully updated, objectID:\"%s\"\r\n", STR_OBJECT_ID);
    }

    if (kiiObj_partiallyUpdate(STR_BUCKET, STR_JSONOBJECT_PARTIALLY_UPDATE, objectID) < 0)
    {
        printf("kii object partially update error!\r\n");
        //return WM_FAILED;
    }
    else
    {
        printf("kii object is partially updated, objectID:\"%s\"\r\n", objectID);
    }
	
    if (kiiObj_fullyUpdate(STR_BUCKET, STR_JSONOBJECT_FULLY_UPDATE, objectID) < 0)
    {
        printf("kii object fully update error!\r\n");
        //return WM_FAILED;
    }
    else
    {
        printf("kii object is fully updated, objectID:\"%s\"\r\n", objectID);
    }

	

/*	
    if (kiiDev_checkRegistered() == 0)
    {
        printf("has registered, create object.\r\n");
        memset(objectID, 0, sizeof(objectID));
        if (kiiObj_create(STR_BUCKET, STR_JSONOBJECT, objectID) < 0)
	{
	    printf("kii create object error!\r\n");
	    return WM_FAILED;
	}
	else
	{
	    printf("kii object is created, ID=%s\r\n", objectID);
	}
    }
*/    
    return WM_SUCCESS;
}


