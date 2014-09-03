#ifndef KII_H
#define KII_H


extern void kii_init(char *site, char *appID, char *appKey);
extern int kiiDev_checkRegistered(void);
extern int kiiDev_getToken(char *deviceVendorID, char *password);
extern int kiiDev_register(char *vendorDeviceID, char *deviceType, unsigned char *password);
extern int kiiObj_create(char *bucketName, char *jsonObject, char *objectID);

#endif

