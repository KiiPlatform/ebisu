#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii_util.h"

char* prv_kii_util_get_http_body(char* str, size_t len)
{
    char* ret = NULL;

    ret = strnstr(str, "\r\n\r\n", len);
    if (ret != NULL) {
        ret += 4;
    }

    return ret;
}

