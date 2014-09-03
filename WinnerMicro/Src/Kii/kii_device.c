#include <string.h>

#include "kii_def.h"
#include "kii_device.h"


extern kii_meta_struct g_kiiMeta;

int kiiDev_checkRegistered(void)
{
    if (strlen(g_kiiMeta.mark) > 0 && strlen(g_kiiMeta.accessToken) > 0 
		&& strlen(g_kiiMeta.deviceID) > 0 && stlen(g_kiiMeta.deviceVendorID) > 0
		&& strlen(g_kiiMeta.password) > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
		
}


int kiiDev_getToken(char *deviceVendorID, char *password)
{
    memset(g_kiiMeta.accessToken, 0, sizeof(g_kiiMeta.accessToken));
    strcpy(g_kiiMeta.accessToken, "")
}

int kiiDev_register(char *vendorDeviceID, char *deviceType, unsigned char *password)
{
    memset(g_kiiMeta.accessToken, 0, sizeof(g_kiiMeta.accessToken));
    strcpy(g_kiiMeta.accessToken, "")
}


