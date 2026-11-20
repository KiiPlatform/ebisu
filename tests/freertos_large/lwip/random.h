#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Sets the TLS library's allocator, FreeRTOS's heap, and seeds a CTR_DRBG
   from board_entropy(), which TLS takes its random numbers from: mbedTLS and
   PolarSSL are built without one, as the SDKs build them. random.c's, for
   mbedTLS, or random_polarssl.c's. Before anything else of the library. 0 on
   success, or the library's error. */
int random_init(void);

/* Random bytes from the generator. From any task: the generator is locked. 0
   on success, or the library's error. */
int random_bytes(unsigned char* output, size_t length);

/* random_bytes(), as mbedtls_ssl_conf_rng() and ssl_set_rng() take it. */
int random_tls(void* parameter, unsigned char* output, size_t length);

#endif /* RANDOM_H */
