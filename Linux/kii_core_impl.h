#ifndef __kii_core_impl
#define __kii_core_impl

#ifdef __cplusplus
extern 'C' {
#endif

#include <openssl/ssl.h>

typedef struct context_t
{
    SSL *ssl;
    SSL_CTX *ssl_ctx;
} context_t;

void
    logger_cb(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
