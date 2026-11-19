#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Random numbers for TLS, which the SDK builds mbedTLS without a generator
   of its own for: mbedTLS's CTR_DRBG, of the sample's own, seeded from the
   noise of the chip's ADC, as the SDK's Gen_RandomSeed() samples it. Not
   rtw_get_random_bytes(), which the SDK's TLS examples take: see README.md.

   Before the Wi-Fi and the scheduler start. 0, or mbedTLS's error, or
   MBEDTLS_ERR_ENTROPY_SOURCE_FAILED where the chip's package is one the SDK
   doesn't sample the ADC on, or -1 without the heap for its lock. */
int random_init(void);

/* As mbedTLS's f_rng: length bytes into output. 0, or mbedTLS's error. From
   any task, once random_init() has succeeded. */
int sample_random(void* unused, unsigned char* output, size_t length);

#endif /* RANDOM_H */
