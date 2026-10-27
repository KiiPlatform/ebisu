#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Seeds the random generator, mbedTLS's CTR_DRBG, from board_entropy(). Before
   the scheduler starts. 0 on success, or an mbedTLS error. */
int random_init(void);

/* Random bytes, for TLS and for FreeRTOS-Plus-TCP's sequence numbers and IDs.
   From any task: the generator is locked. As mbedTLS takes a random generator,
   the first argument unused: 0 on success, or an mbedTLS error. */
int random_bytes(void* unused, unsigned char* output, size_t length);

#endif /* RANDOM_H */
