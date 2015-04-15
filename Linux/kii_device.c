#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_device.h"

#include "kii-core/kii.h"
#include "kii_core_impl.h"

extern kii_t g_kii_data;

/*****************************************************************************
*
*  kiiDev_getToken
*
*  \param  vendorDeviceID - the input of identification of the device
*               password - the input of password
*
*  \return 0:success; -1: failure
*
*  \brief  Gets token
*
*****************************************************************************/
int kiiDev_getToken(char* vendorDeviceID, char* password)
{
	char* p1;
	char* p2;
	char* buf;
	int ret = -1;
    kii_error_code_t core_err;
    kii_author_t author;
    kii_state_t state;

    /* TODO: use wrapper. */
    buf = malloc(4096);
    g_kii_data.http_context->buffer = buf;
    g_kii_data.http_context->buffer_size = 4096;

    core_err = kii_thing_authentication(&g_kii_data, vendorDeviceID, password);
    if (core_err != KIIE_OK) {
        goto free;
    }
    do {
        core_err = kii_run(&g_kii_data);
        state = kii_get_state(&g_kii_data);
    } while (state != KII_STATE_IDLE);
    printf("resp: %s", g_kii_data.response_body);
    if (core_err != KIIE_OK) {
        goto free;
    }
	if(g_kii_data.response_code < 200 || 300 <= g_kii_data.response_code)
	{
		goto free;
	}
	// get access token
	p1 = strstr(buf, "\"access_token\"");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = strstr(p1, ":");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 = strstr(p1, "\"");
	if(p1 == NULL)
	{
		goto free;
	}
	p1 += 1;
	p2 = strstr(p1, "\"");
	if(p2 == NULL)
	{
		goto free;
	}
    memset(&author, 0, sizeof(kii_author_t));
    strcpy(author.author_id, vendorDeviceID);
    memcpy(author.access_token, p1, p2 - p1);
    memcpy(g_kii_data.author, &author, sizeof(kii_author_t));

	ret = 0;
free:
    /* TODO: use wrapper. */
	free(buf);
exit:
	return ret;
}

