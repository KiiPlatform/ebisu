/* Random numbers for TLS, with mbedTLS 2.4.0 as the SDKs configure it, which
   leaves CTR_DRBG and the entropy module out: CTR_DRBG built here, in this
   file alone, as the AmebaZ sample builds it, seeded from board_entropy(),
   under a FreeRTOS mutex, as mbedTLS is built without MBEDTLS_THREADING_C and
   the tests' tasks connect at once. And mbedTLS's
   allocator, FreeRTOS's heap, as the SDKs build it with
   MBEDTLS_PLATFORM_MEMORY and no allocator of its own. See random.h. */
#include "random.h"

#include <limits.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

/* mbedTLS's configuration, mbedtls_config.h, and CTR_DRBG on top of it. */
#include MBEDTLS_CONFIG_FILE
#define MBEDTLS_CTR_DRBG_C
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>
/* mbedTLS's own, from its library's sources, which CMakeLists.txt puts on
   this file's include path. */
#include "ctr_drbg.c"

#include "board.h"

static mbedtls_ctr_drbg_context drbg;
static SemaphoreHandle_t lock;

static void* heap_calloc(size_t count, size_t size)
{
    if (size != 0 && count > SIZE_MAX / size) {
        return NULL;
    }
    void* p = pvPortMalloc(count * size);
    if (p != NULL) {
        memset(p, 0, count * size);
    }
    return p;
}

static int entropy(void* data, unsigned char* output, size_t length)
{
    (void)data;
    return board_entropy(output, length) == 0 ? 0 : MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
}

int random_init(void)
{
    mbedtls_platform_set_calloc_free(heap_calloc, vPortFree);
    lock = xSemaphoreCreateMutex();
    if (lock == NULL) {
        return -1;
    }
    static const unsigned char personalization[] = "ebisu large tests";
    mbedtls_ctr_drbg_init(&drbg);
    int ret = mbedtls_ctr_drbg_seed(&drbg, entropy, NULL, personalization,
            sizeof(personalization) - 1);
    return ret;
}

int random_bytes(unsigned char* output, size_t length)
{
    (void)xSemaphoreTake(lock, portMAX_DELAY);
    int ret = mbedtls_ctr_drbg_random(&drbg, output, length);
    (void)xSemaphoreGive(lock);
    return ret;
}

int random_tls(void* parameter, unsigned char* output, size_t length)
{
    (void)parameter;
    return random_bytes(output, length);
}
