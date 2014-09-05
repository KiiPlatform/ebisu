#include <string.h>
#include "wm_include.h"

#include "kii_meta.h"

kii_meta_struct g_kiiMeta;


/*****************************************************************************
*
*  kii_meta_init
*
*  \param  None
*
*  \return None
*
*  \brief  Initialize the NV meta structure
*
*****************************************************************************/
void kii_meta_init(void)
{
    memset(&g_kiiMeta, 0, sizeof(g_kiiMeta));
    kii_meta_read();
    if (strncmp(g_kiiMeta.mark, KIIMARK, META_MARK_SIZE) != 0)
    {
        memset(&g_kiiMeta, 0, sizeof(g_kiiMeta));
    }
#if 1 //just for test
    strcpy(g_kiiMeta.mark, "kiicloud");
    strcpy(g_kiiMeta.accessToken, "ghhRrxuUfVyyp7ZXaidXe6F00MuFbVrfsVgONXI1gyE");
    strcpy(g_kiiMeta.deviceID, "test1");
    strcpy(g_kiiMeta.deviceVendorID, "abcdef");
    strcpy(g_kiiMeta.password, "123456");
#endif
}

/*****************************************************************************
*
*  kii_meta_write
*
*  \param  None
*
*  \return None
*
*  \brief  Writes meta information to meta memory
*
*****************************************************************************/
void kii_meta_write(void)
{
    tls_fls_write(TLS_FLASH_KII_META_ADDR, (u8*)&g_kiiMeta, sizeof(g_kiiMeta));
}

/*****************************************************************************
*
*  kii_meta_read
*
*  \param  None
*
*  \return None
*
*  \brief  Read meta information from meta memory
*
*****************************************************************************/
void kii_meta_read(void)
{
    tls_fls_read(TLS_FLASH_KII_META_ADDR, (u8*)&g_kiiMeta, sizeof(g_kiiMeta));
}


