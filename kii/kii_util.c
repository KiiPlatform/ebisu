#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii_util.h"

char* prv_kii_util_get_http_body(char* str)
{
    char* ret = NULL;

    ret = strstr(str, "\r\n\r\n");
    if (ret != NULL) {
        ret += 4;
    }

    return ret;
}

