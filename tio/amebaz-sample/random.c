/* Random numbers for TLS: mbedTLS's CTR_DRBG, which the SDK's configuration
   of mbedTLS leaves out, built here for the sample alone, and seeded from the
   noise of the chip's ADC. See random.h and README.md. */
#include "random.h"

#include <limits.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

/* The SDK's configuration of mbedTLS, and CTR_DRBG on top of it, in this file
   alone: its context is the sample's, and nothing else of mbedTLS changes. */
#include <mbedtls/config.h>
#define MBEDTLS_CTR_DRBG_C
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/sha256.h>
/* mbedTLS's own, from the SDK's sources, which build.sh puts on the include
   path. */
#include "ctr_drbg.c"

#include <ameba_soc.h>
#include <system_8195a.h>

/* Bytes of the ADC's noise for each byte of the seed: 8 of its bits for each
   one, as how much entropy each has is not known. */
#define NOISE_PER_BYTE 8

static mbedtls_ctr_drbg_context drbg;
static SemaphoreHandle_t drbg_lock;

/* As CTR_DRBG asks for its entropy: length bytes, each from SHA-256 over 8
   times as many of the ADC's noise, a 32-bit word of it from each
   Gen_RandomSeed(), which takes the ADC's least significant bit 32 times. */
static int adc_entropy(void* unused, unsigned char* output, size_t length)
{
    (void)unused;
    while (length > 0) {
        unsigned char block[32];
        size_t n = length < sizeof(block) ? length : sizeof(block);
        mbedtls_sha256_context sha;
        size_t words;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
        for (words = 0; words < sizeof(block) * NOISE_PER_BYTE / 4; ++words) {
            u32 noise = Gen_RandomSeed();
            mbedtls_sha256_update(&sha, (const unsigned char*)&noise, sizeof(noise));
        }
        mbedtls_sha256_finish(&sha, block);
        mbedtls_sha256_free(&sha);
        memcpy(output, block, n);
        memset(block, 0, sizeof(block));
        output += n;
        length -= n;
    }
    return 0;
}

int random_init(void)
{
    static const unsigned char personalization[] = "ebisu AmebaZ sample";
    int ret;
    /* The SDK's start-up samples the ADC on every package but this one, and
       so does the sample. */
    if (SYSCFG0_BDOption() == SYSCFG_BD_QFN48_MCM_8MBFlash) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
    drbg_lock = xSemaphoreCreateMutex();
    if (drbg_lock == NULL) {
        return -1;
    }
    mbedtls_ctr_drbg_init(&drbg);
    ret = mbedtls_ctr_drbg_seed(&drbg, adc_entropy, NULL, personalization,
            sizeof(personalization) - 1);
    if (ret != 0) {
        return ret;
    }
    /* Seeded once, before the Wi-Fi starts: Gen_RandomSeed() sets the ADC up
       and stops it, which the SDK does only as it starts. 2^31 requests is
       well within the 2^48 a CTR_DRBG may serve on one seed. */
    mbedtls_ctr_drbg_set_reseed_interval(&drbg, INT_MAX);
    return 0;
}

int sample_random(void* unused, unsigned char* output, size_t length)
{
    int ret;
    (void)unused;
    /* The tasks' connections share the generator, and mbedTLS is built
       without MBEDTLS_THREADING_C. */
    xSemaphoreTake(drbg_lock, portMAX_DELAY);
    ret = mbedtls_ctr_drbg_random(&drbg, output, length);
    xSemaphoreGive(drbg_lock);
    return ret;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
