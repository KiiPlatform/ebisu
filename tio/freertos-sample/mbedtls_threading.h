#ifndef MBEDTLS_THREADING_H_SAMPLE
#define MBEDTLS_THREADING_H_SAMPLE

/* Gives mbedTLS FreeRTOS mutexes, for MBEDTLS_THREADING_ALT: once, before
   anything else of mbedTLS is used, psa_crypto_init() among it. The
   connections of the handler, the MQTT task and the updater, on tasks of their
   own, share PSA's random generator. */
void mbedtls_threading_init_freertos(void);

#endif /* MBEDTLS_THREADING_H_SAMPLE */
