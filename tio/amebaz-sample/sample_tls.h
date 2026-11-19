#ifndef SAMPLE_TLS_H
#define SAMPLE_TLS_H

#include <stddef.h>

#include <ebisu_clock.h>
#include <ebisu_log.h>
#include <ebisu_socket.h>

/* The connections' TLS: ebisu's mbedTLS port, over the SDK's mbedTLS, in
   sample_tls_mbedtls.c, or its PolarSSL port, experimental, over the SDK's
   PolarSSL, in sample_tls_polarssl.c, as build.sh builds the sample with
   EBISU_SAMPLE_TLS. */
#if defined(EBISU_SAMPLE_POLARSSL)
#include <ebisu_polarssl.h>
typedef ebisu_polarssl_socket_t sample_tls_socket_t;
#else
#include <ebisu_mbedtls.h>
typedef ebisu_mbedtls_socket_t sample_tls_socket_t;
#endif

/* The connections' TLS configuration: the CA certificates of ca, PEM, with
   its terminating NUL, the random generator of random.h, and records no
   larger than the library takes. Once random_init() has succeeded. On a
   failure, it logs why and stops. */
void sample_tls_init(const char* ca, size_t ca_size);

/* A connection, over transport, of the configuration, whose certificates are
   checked at clock's time, and which logs to logger why it fails. After
   sample_tls_init(). */
void sample_tls_socket_init(sample_tls_socket_t* tls, ebisu_socket_t* transport,
        ebisu_clock_t* clock, ebisu_logger_t* logger);

#endif /* SAMPLE_TLS_H */
