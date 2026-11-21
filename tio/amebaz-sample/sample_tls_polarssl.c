/* The connections' TLS, with ebisu's PolarSSL port, experimental, over the
   SDK's PolarSSL 1.3.8, whose cryptography is in the chip's ROM. See
   sample_tls.h and README.md. */
#include "sample_tls.h"

#include <polarssl/ssl.h>

#include "log.h"
#include "random.h"

/* The connections share it: the CA certificates, and the setup of each
   connection's context. */
static ebisu_polarssl_tls_t tls_config;

/* Each connection's settings of the sample's, which the port calls as it sets
   a connection's context up, PolarSSL 1.3 having no configuration the
   connections share: the random generator, and records no larger than
   PolarSSL takes. */
static void setup(ssl_context* ssl, void* userdata)
{
    (void)userdata;
#if !EBISU_POLARSSL_SETS_RNG
    /* The SDK builds PolarSSL without a CTR_DRBG: see random.h. */
    ssl_set_rng(ssl, sample_random, NULL);
#endif
#if defined(POLARSSL_SSL_MAX_FRAGMENT_LENGTH) && SSL_MAX_CONTENT_LEN < 16384
    /* The SDK's PolarSSL takes records of SSL_MAX_CONTENT_LEN, 5120 bytes,
       where a server may send 16384: the server is asked for records of 4096
       at the most, which one that doesn't take the extension ignores. A
       length of those PolarSSL has, which it takes. */
#if SSL_MAX_CONTENT_LEN >= 4096
    (void)ssl_set_max_frag_len(ssl, SSL_MAX_FRAG_LEN_4096);
#elif SSL_MAX_CONTENT_LEN >= 2048
    (void)ssl_set_max_frag_len(ssl, SSL_MAX_FRAG_LEN_2048);
#elif SSL_MAX_CONTENT_LEN >= 1024
    (void)ssl_set_max_frag_len(ssl, SSL_MAX_FRAG_LEN_1024);
#else
    (void)ssl_set_max_frag_len(ssl, SSL_MAX_FRAG_LEN_512);
#endif
#endif
}

void sample_tls_init(const char* ca, size_t ca_size)
{
    /* PolarSSL's allocator, which random_init() set. */
    int ret = 0;
    if (!ebisu_polarssl_tls_init(&tls_config, (const unsigned char*)ca, ca_size, &ret)) {
        sample_fatal("failed to set up TLS: -0x%04x", (unsigned int)-ret);
    }
    ebisu_polarssl_tls_set_setup(&tls_config, setup, NULL);
}

void sample_tls_socket_init(sample_tls_socket_t* tls, ebisu_socket_t* transport,
        ebisu_clock_t* clock, ebisu_logger_t* logger)
{
    ebisu_polarssl_socket_init(tls, transport, &tls_config);
    ebisu_polarssl_socket_set_clock(tls, clock);
    ebisu_polarssl_socket_set_logger(tls, logger);
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
