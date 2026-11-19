/* The tests' lwIP port to a POSIX host: its compiler and C library. */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>

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
