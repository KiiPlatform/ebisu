#ifndef KII_UTILS_H
#define KII_UTILS_H

#ifndef KII_TOKEN_NUM
#define KII_TOKEN_NUM 64
#endif

#include "kii.h"

int prv_kii_jsmn_get_tokens(
        kii_t* kii,
        const char* javascript,
        size_t javascript_len,
        jsmntok_t* tokens,
        size_t token_num);

int prv_kii_jsmn_get_value(
        const char* javascript,
        size_t javascript_len,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token);

#endif
