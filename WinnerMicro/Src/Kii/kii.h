#ifndef KII_H
#define KII_H

#define KII_SITE_SIZE 2
#define KII_HOST_SIZE 32
#define KII_APPID_SIZE 8
#define KII_APPKEY_SIZE 32

#define KII_ACCESS_TOKEN_SIZE   44
#define KII_DEVICE_VENDOR_ID     64 //matches [a-zA-Z0-9-_\\.]{3,64}
#define KII_DEVICE_ID                  20
#define KII_PASSWORD_SIZE        50  //Matches ^[\\u0020-\\u007E]{4,50}
#define KII_OBJECTID_SIZE 36
#define KII_DATA_TPYE_SIZE 36
#define KII_UPLOAD_ID_SIZE 46
#define KII_BUCKET_NAME_SIZE 20

#define KII_SEND_BUF_SIZE 2048
#define KII_RECV_BUF_SIZE 2048


typedef void (* recvPushMessageCallback)(char* jsonBuf);


extern int kii_init(char *site, char *appID, char *appKey);

extern int kiiDev_getToken(char *deviceVendorID, char *password);
extern int kiiDev_register(char *vendorDeviceID, char *deviceType, char *password);

extern int kiiObj_create(char *bucketName, char *jsonObject, char *dataType, char *objectID);
extern int kiiObj_createWithID(char *bucketName, char *jsonObject, char *dataType, char *objectID);
extern int kiiObj_fullyUpdate(char *bucketName, char *jsonObject, char *dataType, char *objectID);
extern int kiiObj_partiallyUpdate(char *bucketName, char *jsonObject, char *objectID);
extern int kiiObj_uploadBodyAtOnce(char *bucketName, char *objectID,  char *dataType, unsigned char *data, unsigned int length);
extern int kiiObj_uploadBodyInit(char *bucketName, char *objectID, char *dataType, unsigned int totalLength);
extern int kiiObj_uploadBody(unsigned char *data, unsigned int length);
extern int kiiObj_uploadBodyCommit(int committed);
extern int KiiPush_init(unsigned int taskPrio, recvPushMessageCallback callback);


#endif

