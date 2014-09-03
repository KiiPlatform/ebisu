#ifndef KII_META_H
#define KII_META_H


// defines
#define TLS_FLASH_KII_META_ADDR			(0x00099000)

#define KIIMARK "kiicloud"

#define META_MARK_SIZE  8
#define META_ACCESS_TOKEN_SIZE   44
#define META_DEVICE_VENDOR_ID     64 //matches [a-zA-Z0-9-_\\.]{3,64}
#define META_DEVICE_ID                  20
#define META_PASSWORD_SIZE        50  //Matches ^[\\u0020-\\u007E]{4,50}


typedef struct {
    char mark[META_MARK_SIZE];                 // watermark
    char accessToken[META_ACCESS_TOKEN_SIZE+1];
    char deviceVendorID[META_DEVICE_VENDOR_ID+1];
    char deviceID[META_DEVICE_ID+1];
    char password[META_PASSWORD_SIZE+1];
} kii_meta_struct;

// functions for export
void kii_meta_init(void);
void kii_meta_write(void);
void kii_meta_read(void);


#endif

