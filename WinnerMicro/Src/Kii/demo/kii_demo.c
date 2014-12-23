#include <string.h>
#include "wm_include.h"

#include "kii.h"
#include "light.h"
#include "kii_demo.h"


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

    if (kiiObj_create(STR_TEST_BUCKET, STR_TEST_JSONOBJECT, STR_TEST_MEDIA_TYPE, objectID) < 0)
    {
        printf("Create object failed\r\n");
    }
    else
    {
        printf("Create object success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_createWithID(STR_TEST_BUCKET, STR_TEST_JSONOBJECT, STR_TEST_MEDIA_TYPE, STR_TEST_OBJECT_ID) < 0)
    {
        printf("Create object with ID failed\r\n");
    }
    else
    {
        printf("Create object with ID success, objectID:\"%s\"\r\n", STR_TEST_OBJECT_ID);
    }

    if (kiiObj_partiallyUpdate(STR_TEST_BUCKET, STR_TEST_JSONOBJECT_PARTIALLY_UPDATE, objectID) < 0)
    {
        printf("Partially update object failed\r\n");
    }
    else
    {
        printf("Partially updated object success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_fullyUpdate(STR_TEST_BUCKET, STR_TEST_JSONOBJECT_FULLY_UPDATE, STR_TEST_MEDIA_TYPE,  objectID) < 0)
    {
        printf("Fully update object failed\r\n");
    }
    else
    {
        printf("Fully updated object success, objectID:\"%s\"\r\n", objectID);
    }
	
    memset(objectBody, 'T', sizeof(objectBody));
    if (kiiObj_uploadBodyAtOnce(STR_TEST_BUCKET, objectID,  STR_TEST_OBJECTBODY_TYPE, objectBody, sizeof(objectBody)) < 0)
    {
        printf("Upload object body at once failed\r\n");
    }
    else
    {
        printf("Uploaded object body at once success, objectID:\"%s\"\r\n", objectID);
    }

    if (kiiObj_uploadBodyInit(STR_TEST_BUCKET, objectID, STR_TEST_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
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
    if (kiiObj_uploadBodyInit(STR_TEST_BUCKET, objectID, STR_TEST_OBJECTBODY_TYPE, sizeof(objectMultiplePiecesBody1)+sizeof(objectMultiplePiecesBody2)) < 0)
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
    if (kiiObj_retrieve(STR_TEST_BUCKET, objectID, jsonObject, sizeof(jsonObject)) < 0)
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
    if (kiiObj_downloadBody(STR_TEST_BUCKET, objectID,  bodyPosition,  bodyLength, bodyP, &bodyActualLength, &bodyTotalLength) < 0)
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
		if (kiiObj_downloadBody(STR_TEST_BUCKET, objectID,  bodyPosition,  bodyLength, bodyP, &bodyActualLength, &bodyTotalLength) < 0)
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

void kiiDemo_testTopic(void)
{
    if (kiiPush_createTopic(STR_TEST_THING_TOPIC) < 0)
    {
 	printf("Create thing topic failed, topic:\"%s\"\r\n", STR_TEST_THING_TOPIC);
    }
    else
    {
	printf("Create thing topic success, topic:\"%s\"\r\n", STR_TEST_THING_TOPIC);
    }

    if (kiiPush_subscribeTopic(STR_TEST_THING_TOPIC) < 0)
    {
 	printf("Subscribe thing topic failed, topic:\"%s\"\r\n", STR_TEST_THING_TOPIC);
    }
    else
    {
	printf("Subscribe thing topic success, topic:\"%s\"\r\n", STR_TEST_THING_TOPIC);
    }
}



int kiiDemo_test(char *buf)
{
    (void) buf;
	
    if (kii_init(STR_SITE, STR_APPID, STR_APPKEY) < 0)
    {
       printf("Initialize kii failed\r\n");
       return 0;
    }

    if (light_onBoarding() != 0)
    {
    	    printf("Device onbording failed\r\n");
	    return 0;
    }

    //kiiDemo_testObject();
    //kiiDemo_testTopic();

    if (light_initPush() < 0)
    {
        printf("Initialize push failed\r\n");
	return WM_FAILED;
    }
    else
    {
	printf("Initialize push success\r\n");
	return WM_SUCCESS;
    }
}


