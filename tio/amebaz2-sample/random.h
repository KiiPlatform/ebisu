#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Random numbers for TLS, which the SDK builds mbedTLS without a generator
   of its own for: from the crypto engine's, crypto_random_generate(), which
   derives what it gives with HKDF from timer jitter it samples for each call.
   Not rtw_get_random_bytes(), which the SDK's TLS examples take: see
   README.md.

   Before the scheduler starts. */
void random_init(void);

/* As mbedTLS's f_rng: length bytes into output. 0, or
   MBEDTLS_ERR_ENTROPY_SOURCE_FAILED if the crypto engine failed. From any
   task. */
int sample_random(void* unused, unsigned char* output, size_t length);

#endif /* RANDOM_H */
