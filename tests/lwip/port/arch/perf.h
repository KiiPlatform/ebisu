/* The tests' lwIP port to a POSIX host: lwIP's performance hooks, which lwIP
   1.4 includes whether or not LWIP_PERF is set, and the tests don't take. */
#ifndef LWIP_ARCH_PERF_H
#define LWIP_ARCH_PERF_H

#define PERF_START
#define PERF_STOP(x)

#endif /* LWIP_ARCH_PERF_H */
