#include <string.h>

#include "kii.h"
#include "kii_def.h"

kii_data_struct g_kii_data;

/*****************************************************************************
*
*  kii_init
*
*  \param  site - the input of site name, should be one of "CN", "JP", "US", "SG"
*              appID - the input of Application ID
*              objectID - the input of Application Key
*
*  \return  0:success; -1: failure
*
*  \brief  Initializes Kii 
*
*****************************************************************************/
int kii_init(char *site, char *appID, char *appKey) {
	memset(&g_kii_data, 0, sizeof(g_kii_data));
	if ((strlen(site) != KII_SITE_SIZE) || (strlen(appID) != KII_APPID_SIZE)
			|| (strlen(appKey) != KII_APPKEY_SIZE)) {
		return -1;
	} else {
		if (strcmp(site, "CN") == 0) {
			strcpy(g_kii_data.host, "api-cn2.kii.com");
		} else if (strcmp(site, "JP") == 0) {
			//strcpy(g_kii_data.host, "api-jp.kii.com");
			strcpy(g_kii_data.host, "api-development-jp.internal.kii.com");
		} else if (strcmp(site, "US") == 0) {
			strcpy(g_kii_data.host, "api.kii.com");
		} else if (strcmp(site, "SG") == 0) {
			strcpy(g_kii_data.host, "api-sg.kii.com");
		} else {
			return -1;
		}

		strcpy(g_kii_data.appID, appID);
		strcpy(g_kii_data.appKey, appKey);

		return 0;
	}
}

