#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Random numbers for TLS, which the SDK builds mbedTLS, and PolarSSL, without
   a generator of its own for: a CTR_DRBG of the sample's own, seeded from the
   noise of the chip's ADC, as the SDK's Gen_RandomSeed() samples it, in
   random_mbedtls.c, of mbedTLS, or random_polarssl.c, of PolarSSL, as build.sh
   builds the sample with one or the other. Not rtw_get_random_bytes(), which
   the SDK's TLS examples take: see README.md.

   Before the Wi-Fi and the scheduler start, and before anything else of the
   TLS library: it sets the library's allocator, FreeRTOS's heap, which
   switches the library's AES to the chip's crypto engine, and starts the
   engine. 0, or the library's error, or its error for an entropy source that
   failed where the chip's package is one the SDK doesn't sample the ADC on,
   or -1 if the engine doesn't start. */
int random_init(void);

/* As the TLS library's f_rng: length bytes into output. 0, or the library's
   error. From any task, once random_init() has succeeded. */
int sample_random(void* unused, unsigned char* output, size_t length);

#endif /* RANDOM_H */
