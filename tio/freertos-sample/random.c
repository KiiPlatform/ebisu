#include "random.h"

#include <mbedtls/entropy.h>
#include <psa/crypto.h>

#include "board.h"

/* mbedTLS's entropy source, with MBEDTLS_ENTROPY_HARDWARE_ALT, which PSA's
   random generator is seeded from. Declared in none of mbedTLS's public
   headers. */
int mbedtls_hardware_poll(void* data, unsigned char* output, size_t length,
        size_t* out_length);

int mbedtls_hardware_poll(void* data, unsigned char* output, size_t length,
        size_t* out_length)
{
    (void)data;
    if (board_entropy(output, length) != 0) {
        *out_length = 0;
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    *out_length = length;
    return 0;
}

int random_init(void)
{
    return (int)psa_crypto_init();
}

int random_bytes(unsigned char* output, size_t length)
{
    return (int)psa_generate_random(output, length);
}
