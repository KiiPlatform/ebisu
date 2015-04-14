#ifndef __kii_core_impl
#define __kii_core_impl

#ifdef __cplusplus
extern 'C' {
#endif

#include "kii-core/kii.h"
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef enum prv_ssl_state_t {
    PRV_SSL_STATE_IDLE,
    PRV_SSL_STATE_CONNECT,
    PRV_SSL_STATE_SEND,
    PRV_SSL_STATE_RECV,
    PRV_SSL_STATE_CLOSE
} prv_ssl_state_t;
    
typedef struct context_t
{
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    int sock;
    char* buff;
    size_t buff_size;
    char host[256];
    prv_ssl_state_t state;
    int last_chunk;
    int sent_size;
    int received_size;
} context_t;

kii_http_client_code_t
    request_line_cb(
            void* http_context,
            const char* method,
            const char* host,
            const char* path);

kii_http_client_code_t
    header_cb(
            void* http_context,
            const char* key,
            const char* value);

kii_http_client_code_t
    body_cb(
            void* http_context,
            const char* body_data);

kii_http_client_code_t
    execute_cb(
            void* http_context,
            int* response_code,
            char** response_body);

void
    logger_cb(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
