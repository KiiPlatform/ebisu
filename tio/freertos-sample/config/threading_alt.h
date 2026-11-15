#ifndef THREADING_ALT_H
#define THREADING_ALT_H

/* mbedTLS's mutex with MBEDTLS_THREADING_ALT: a FreeRTOS mutex, made by
   mbedtls_threading.c's functions, which main() gives mbedTLS with
   mbedtls_threading_set_alt() before anything else of it is used. Left
   opaque, so that mbedTLS's sources need nothing of FreeRTOS. */
typedef struct mbedtls_threading_mutex_t {
    void* mutex;
} mbedtls_threading_mutex_t;

#endif /* THREADING_ALT_H */
