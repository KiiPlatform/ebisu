/* Random numbers for TLS, with PolarSSL 1.3.8 as the SDK for AmebaZ
   configures it, which leaves CTR_DRBG and the entropy module out: CTR_DRBG
   built here, in this file alone, where the AmebaZ sample takes the ROM's,
   seeded from board_entropy(), under a FreeRTOS mutex, as PolarSSL is built
   without POLARSSL_THREADING_C and the tests' tasks connect at once. And
   PolarSSL's allocator, FreeRTOS's heap, as the sample sets it. See
   random.h. */
#include "random.h"

#include <limits.h>

#include <FreeRTOS.h>
#include <semphr.h>

/* PolarSSL's configuration, polarssl_config.h, and CTR_DRBG on top of it. */
#include POLARSSL_CONFIG_FILE
#define POLARSSL_CTR_DRBG_C
#include <polarssl/ctr_drbg.h>
#include <polarssl/entropy.h>
#include <polarssl/platform.h>
/* PolarSSL's own, from its library's sources, which CMakeLists.txt puts on
   this file's include path. */
#include "ctr_drbg.c"

#include "board.h"

static ctr_drbg_context drbg;
static SemaphoreHandle_t lock;

static int entropy(void* data, unsigned char* output, size_t length)
{
    (void)data;
    return board_entropy(output, length) == 0 ? 0 : POLARSSL_ERR_ENTROPY_SOURCE_FAILED;
}

int random_init(void)
{
    platform_set_malloc_free(pvPortMalloc, vPortFree);
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
        return -1;
    }
    static const unsigned char personalization[] = "ebisu large tests";
    return ctr_drbg_init(&drbg, entropy, NULL, personalization, sizeof(personalization) - 1);
}

int random_bytes(unsigned char* output, size_t length)
{
    (void)xSemaphoreTake(lock, portMAX_DELAY);
    int ret = ctr_drbg_random(&drbg, output, length);
    (void)xSemaphoreGive(lock);
    return ret;
}

int random_tls(void* parameter, unsigned char* output, size_t length)
{
    (void)parameter;
    return random_bytes(output, length);
}
