/* The tests' lwIP port to a POSIX host: its compiler and C library. */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>

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

/* The host's struct timeval, which SO_RCVTIMEO and SO_SNDTIMEO take without
   LWIP_SO_SNDRCVTIMEO_NONSTANDARD. */
#define LWIP_TIMEVAL_PRIVATE 0
#include <sys/time.h>

/* The host's errno, which lwIP's sockets set: included here, as lwIP 2.0
   would include LWIP_ERRNO_INCLUDE with glibc's errno macro expanded in its
   name, and by lwIP from 2.1 on with LWIP_ERRNO_STDINCLUDE. */
#include <errno.h>
#define LWIP_ERRNO_STDINCLUDE 1

#define LWIP_RAND() ((u32_t)rand())

#define LWIP_PLATFORM_DIAG(x) do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x) do { \
        fprintf(stderr, "lwIP assertion \"%s\" failed at %s:%d\n", x, __FILE__, __LINE__); \
        abort(); \
    } while (0)

typedef int sys_prot_t;

#endif /* LWIP_ARCH_CC_H */
