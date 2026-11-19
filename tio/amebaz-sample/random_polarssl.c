/* Random numbers for TLS, with PolarSSL: its CTR_DRBG, which the SDK's
   configuration of PolarSSL leaves out of what it compiles, and the chip's ROM
   has, seeded from the noise of the chip's ADC. See random.h and README.md. */
#include "random.h"

#include <limits.h>
#include <string.h>

#include <FreeRTOS.h>

#include <polarssl/ctr_drbg.h>
#include <polarssl/platform.h>
#include <polarssl/sha256.h>

#include <ameba_soc.h>
#include <device_lock.h>
#include <system_8195a.h>

/* Bytes of the ADC's noise for each byte of the seed: 8 of its bits for each
   one, as how much entropy each has is not known. */
#define NOISE_PER_BYTE 8

static ctr_drbg_context drbg;

/* As CTR_DRBG asks for its entropy: length bytes, each from SHA-256 over 8
   times as many of the ADC's noise, a 32-bit word of it from each
   Gen_RandomSeed(), which takes the ADC's least significant bit 32 times. */
static int adc_entropy(void* unused, unsigned char* output, size_t length)
{
    (void)unused;
    while (length > 0) {
        unsigned char block[32];
        size_t n = length < sizeof(block) ? length : sizeof(block);
        sha256_context sha;
        size_t words;
        sha256_init(&sha);
        sha256_starts(&sha, 0);
        for (words = 0; words < sizeof(block) * NOISE_PER_BYTE / 4; ++words) {
            u32 noise = Gen_RandomSeed();
            sha256_update(&sha, (const unsigned char*)&noise, sizeof(noise));
        }
        sha256_finish(&sha, block);
        sha256_free(&sha);
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
        return POLARSSL_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    }
    /* PolarSSL's allocations, from FreeRTOS's heap, and its AES, the ROM's,
       in the chip's crypto engine, as the SDK sets PolarSSL up: before the
       generator is seeded, as it keeps its AES key as the engine takes it.
       The SDK starts the engine as it sets each connection up: here, it
       starts it for the generator, before any connection. */
    platform_set_malloc_free(pvPortMalloc, vPortFree);
    if (rtl_cryptoEngine_init() != 0) {
        return -1;
    }
    ret = ctr_drbg_init(&drbg, adc_entropy, NULL, personalization,
            sizeof(personalization) - 1);
    if (ret != 0) {
        return ret;
    }
    /* Seeded once, before the Wi-Fi starts: Gen_RandomSeed() sets the ADC up
       and stops it, which the SDK does only as it starts. 2^31 requests is
       well within the 2^48 a CTR_DRBG may serve on one seed. */
    ctr_drbg_set_reseed_interval(&drbg, INT_MAX);
    return 0;
}

int sample_random(void* unused, unsigned char* output, size_t length)
{
    int ret;
    (void)unused;
    /* The tasks' connections share the generator, and its AES runs in the
       chip's crypto engine, as theirs does: under the SDK's lock of the
       engine, which PolarSSL's ciphers take as they run in it, and which is
       never held while PolarSSL asks for random numbers. */
    device_mutex_lock(RT_DEV_LOCK_CRYPTO);
    ret = ctr_drbg_random(&drbg, output, length);
    device_mutex_unlock(RT_DEV_LOCK_CRYPTO);
    return ret;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
