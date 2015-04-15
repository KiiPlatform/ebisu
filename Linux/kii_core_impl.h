#ifndef __kii_core_impl
#define __kii_core_impl

#ifdef __cplusplus
extern 'C' {
#endif

#include "kii-core/kii.h"
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

kii_http_client_code_t
    request_line_cb(
            kii_http_context_t* http_context,
            const char* method,
            const char* host,
            const char* path);

kii_http_client_code_t
    header_cb(
            kii_http_context_t* http_context,
            const char* key,
            const char* value);

kii_http_client_code_t
    body_cb(
            kii_http_context_t* http_context,
            const char* body_data);

kii_http_client_code_t
    execute_cb(
            kii_http_context_t* http_context,
            int* response_code,
            char** response_body);

void
    logger_cb(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
