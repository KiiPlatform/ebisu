/* Holds back lookups, for tls_test.py, as name servers that don't answer
 * would: preloaded into wine, whose Winsock looks hosts up with the C
 * library's getaddrinfo(), it takes the place of that.
 *
 * A host <ms>.lookup-gate.invalid is looked up as localhost, ms milliseconds
 * later. Any other is the C library's to look up.
 *
 *     cc -shared -fPIC -o lookup_gate.so lookup_gate.c -ldl
 *     LD_PRELOAD=./lookup_gate.so wine ...
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char GATED_DOMAIN[] = ".lookup-gate.invalid";

typedef int (*getaddrinfo_t)(const char*, const char*, const struct addrinfo*,
        struct addrinfo**);

int getaddrinfo(const char* host, const char* service,
        const struct addrinfo* hints, struct addrinfo** out)
{
    getaddrinfo_t libc = (getaddrinfo_t)dlsym(RTLD_NEXT, "getaddrinfo");
    size_t length = host == NULL ? 0 : strlen(host);
    size_t domain = sizeof(GATED_DOMAIN) - 1;
    if (length <= domain || strcmp(host + length - domain, GATED_DOMAIN) != 0) {
        return libc(host, service, hints, out);
    }
    long ms = strtol(host, NULL, 10);
    struct timespec delay;
    delay.tv_sec = ms / 1000;
    delay.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&delay, &delay) != 0) {
    }
    return libc("localhost", service, hints, out);
}
