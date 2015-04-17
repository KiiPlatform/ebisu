#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii.h"
#include "kii_def.h"
#include "kii_device.h"

#include "kii-core/kii.h"
#include "kii_core_impl.h"

int kiiDev_getToken(kii_t* kii, char* vendorDeviceID, char* password)
{
	char* p1;
	char* p2;
	char* buf;
	int ret = -1;
    kii_error_code_t core_err;
    kii_state_t state;

    buf = kii->http_context.buffer;
    core_err = kii_thing_authentication(kii, vendorDeviceID, password);
    if (core_err != KIIE_OK) {
        goto exit;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    M_KII_LOG(kii->logger_cb("resp: %s\n", kii->response_body));
    if (core_err != KIIE_OK) {
        goto exit;
    }
	if(kii->response_code < 200 || 300 <= kii->response_code)
	{
		goto exit;
	}
	// get access token
	p1 = strstr(buf, "\"access_token\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, ":");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 = strstr(p1, "\"");
	if(p1 == NULL)
	{
		goto exit;
	}
	p1 += 1;
	p2 = strstr(p1, "\"");
	if(p2 == NULL)
	{
		goto exit;
	}
    strcpy(kii->author.author_id, vendorDeviceID);
    memcpy(kii->author.access_token, p1, p2 - p1);
	ret = 0;

exit:
	return ret;
}

