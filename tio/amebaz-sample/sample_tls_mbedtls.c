/* The connections' TLS, with ebisu's mbedTLS port, over the SDK's mbedTLS
   2.4.0. See sample_tls.h and README.md. */
#include "sample_tls.h"

#include <mbedtls/ssl.h>

#include "log.h"
#include "random.h"

/* The connections share it: the CA certificates, and mbedTLS's
   configuration. */
static ebisu_mbedtls_tls_t tls_config;

/* mbedTLS's allocator is random_init()'s. */
void sample_tls_init(const char* ca, size_t ca_size)
{
    mbedtls_ssl_config* conf;
    int ret;

    if (!ebisu_mbedtls_tls_init(&tls_config, (const unsigned char*)ca, ca_size, &ret)) {
        sample_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }
    conf = ebisu_mbedtls_tls_config(&tls_config);
#if !EBISU_MBEDTLS_SETS_RNG
    /* The SDK builds mbedTLS without a CTR_DRBG: see random.h. */
    mbedtls_ssl_conf_rng(conf, sample_random, NULL);
#endif
#if defined(MBEDTLS_SSL_MAX_FRAGMENT_LENGTH) && defined(MBEDTLS_SSL_MAX_CONTENT_LEN) && \
    MBEDTLS_SSL_MAX_CONTENT_LEN < 16384
    /* The SDK's mbedTLS takes records of MBEDTLS_SSL_MAX_CONTENT_LEN, 4096
       bytes, where a server may send 16384: the server is asked for records
       no larger, which one that doesn't take the extension ignores. */
#if MBEDTLS_SSL_MAX_CONTENT_LEN >= 4096
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_4096);
#elif MBEDTLS_SSL_MAX_CONTENT_LEN >= 2048
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_2048);
#elif MBEDTLS_SSL_MAX_CONTENT_LEN >= 1024
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_1024);
#else
    ret = mbedtls_ssl_conf_max_frag_len(conf, MBEDTLS_SSL_MAX_FRAG_LEN_512);
#endif
    if (ret != 0) {
        sample_fatal("failed to set the record size: -0x%04x", (unsigned int)-ret);
    }
#else
    (void)conf;
#endif
}

void sample_tls_socket_init(sample_tls_socket_t* tls, ebisu_socket_t* transport,
        ebisu_clock_t* clock, ebisu_logger_t* logger)
{
    ebisu_mbedtls_socket_init(tls, transport, &tls_config);
    ebisu_mbedtls_socket_set_clock(tls, clock);
    ebisu_mbedtls_socket_set_logger(tls, logger);
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
