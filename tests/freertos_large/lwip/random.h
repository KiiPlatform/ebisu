#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Sets mbedTLS's allocator, FreeRTOS's heap, and seeds a CTR_DRBG from
   board_entropy(), which TLS takes its random numbers from: mbedTLS is built
   without one, as the SDKs build it. Before anything else of mbedTLS. 0 on
   success, or mbedTLS's error. */
int random_init(void);

/* Random bytes from the generator. From any task: the generator is locked. 0
   on success, or mbedTLS's error. */
int random_bytes(unsigned char* output, size_t length);

/* random_bytes(), as mbedtls_ssl_conf_rng() takes it. */
int random_tls(void* parameter, unsigned char* output, size_t length);

#endif /* RANDOM_H */
