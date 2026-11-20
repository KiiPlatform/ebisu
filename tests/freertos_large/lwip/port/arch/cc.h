/* The large tests' lwIP port to FreeRTOS on the board: its compiler and C
   library, GCC's and newlib, for the Cortex-M3. */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdlib.h>
#include <sys/time.h>

#if TEST_LWIP_1
/* lwIP 1.4 takes its integer types and their printf formats from here, and
   lwIP 2.0 on from stdint.h and inttypes.h itself. */
#include <inttypes.h>
#include <stdint.h>
typedef uint8_t u8_t;
typedef int8_t s8_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef uintptr_t mem_ptr_t;
#define U16_F PRIu16
#define S16_F PRId16
#define X16_F PRIx16
#define U32_F PRIu32
#define S32_F PRId32
#define X32_F PRIx32
#define SZT_F "zu"
/* Its packed structures, which 2.0 on default to these. */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* newlib's struct timeval, which lwIP's select() takes, rather than one of
   lwIP's own, which newlib's headers would clash with. */
#define LWIP_TIMEVAL_PRIVATE 0

/* As the SDKs have it. */
#define LWIP_RAND() ((u32_t)rand())

/* board.c's. */
void board_fatal(const char* format, ...) __attribute__((noreturn, format(printf, 1, 2)));
#define LWIP_PLATFORM_DIAG(x) do { } while (0)
#define LWIP_PLATFORM_ASSERT(x) board_fatal("lwIP assertion \"%s\" failed at %s:%d", \
        x, __FILE__, __LINE__)

#endif /* LWIP_ARCH_CC_H */
