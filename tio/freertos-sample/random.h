#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Starts PSA, whose random generator mbedTLS takes for TLS, and which it seeds
   from board_entropy(), through MBEDTLS_ENTROPY_HARDWARE_ALT. After
   mbedtls_threading_init_freertos(), and before the scheduler starts. 0 on
   success, or PSA's error. */
int random_init(void);

/* Random bytes from PSA's generator, for FreeRTOS-Plus-TCP's sequence numbers
   and IDs. From any task: the generator is locked. 0 on success, or PSA's
   error. */
int random_bytes(unsigned char* output, size_t length);

#endif /* RANDOM_H */
