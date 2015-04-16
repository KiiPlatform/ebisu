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

    /* TODO: use wrapper. */
    buf = malloc(4096);
    kii->http_context.buffer = buf;
    kii->http_context.buffer_size = 4096;

    core_err = kii_thing_authentication(kii, vendorDeviceID, password);
    if (core_err != KIIE_OK) {
        goto free;
    }
    do {
        core_err = kii_run(kii);
        state = kii_get_state(kii);
    } while (state != KII_STATE_IDLE);
    printf("resp: %s", kii->response_body);
    if (core_err != KIIE_OK) {
        goto free;
    }
	if(kii->response_code < 200 || 300 <= kii->response_code)
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
    strcpy(kii->author.author_id, vendorDeviceID);
    memcpy(kii->author.access_token, p1, p2 - p1);

	ret = 0;
free:
    /* TODO: use wrapper. */
	free(buf);
exit:
	return ret;
}

