#include "random.h"

#include <FreeRTOS.h>
#include <semphr.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include "board.h"

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context generator;
static SemaphoreHandle_t generator_lock;

/* mbedTLS's entropy source, with MBEDTLS_ENTROPY_HARDWARE_ALT. Declared in
   none of mbedTLS's public headers. */
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
    static const char personalization[] = "ebisu FreeRTOS sample";
    generator_lock = xSemaphoreCreateMutex();
    configASSERT(generator_lock != NULL);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&generator);
    return mbedtls_ctr_drbg_seed(&generator, mbedtls_entropy_func, &entropy,
            (const unsigned char*)personalization, sizeof(personalization) - 1);
}

int random_bytes(void* unused, unsigned char* output, size_t length)
{
    (void)unused;
    xSemaphoreTake(generator_lock, portMAX_DELAY);
    int ret = mbedtls_ctr_drbg_random(&generator, output, length);
    xSemaphoreGive(generator_lock);
    return ret;
}
