#include "random.h"

#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#include <crypto_api.h>
#include <device_lock.h>

#include <mbedtls/entropy.h>

#include "log.h"

/* What each call of crypto_random_generate() gives, from timer jitter it
   samples afresh, which takes a few hundred of the timer's 31-microsecond
   ticks: TLS takes its random numbers a few bytes at a time, as many as
   PKCS #1 v1.5's padding has one byte at a time. */
#define POOL_SIZE 256

static unsigned char pool[POOL_SIZE];
/* How many of pool's bytes, at its end, are yet to be given. */
static size_t pool_left;
static SemaphoreHandle_t pool_lock;

void random_init(void)
{
    pool_lock = xSemaphoreCreateMutex();
    configASSERT(pool_lock != NULL);
}

int sample_random(void* unused, unsigned char* output, size_t length)
{
    int ret = 0;
    (void)unused;
    xSemaphoreTake(pool_lock, portMAX_DELAY);
    while (length > 0) {
        unsigned char* from;
        size_t n;
        if (pool_left == 0) {
            /* The engine's, which mbedTLS's hardware AES uses too. The SDK
               starts it before main(). */
            device_mutex_lock(RT_DEV_LOCK_CRYPTO);
            ret = crypto_random_generate(pool, POOL_SIZE);
            device_mutex_unlock(RT_DEV_LOCK_CRYPTO);
            if (ret != 0) {
                sample_log("the crypto engine gave no random numbers: %d", ret);
                ret = MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
                break;
            }
            pool_left = POOL_SIZE;
        }
        n = length < pool_left ? length : pool_left;
        from = pool + POOL_SIZE - pool_left;
        memcpy(output, from, n);
        /* Each byte is given once. */
        memset(from, 0, n);
        pool_left -= n;
        output += n;
        length -= n;
    }
    xSemaphoreGive(pool_lock);
    return ret;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
