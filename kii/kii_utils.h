#ifndef KII_UTILS_H
#define KII_UTILS_H

int prv_kii_jsmn_get_tokens(
        const char* javascript,
        size_t javascript_len,
        jsmntok_t** out_tokens);

int prv_kii_jsmn_get_value(
        const char* javascript,
        size_t javascript_len,
        const jsmntok_t* tokens,
        const char* name,
        jsmntok_t** out_token);

#endif
