#include "ebisu_retry.h"

#include <stddef.h>

/* 32-bit FNV-1a's offset basis and prime. */
#define _FNV_BASIS 2166136261U
#define _FNV_PRIME 16777619U

uint32_t ebisu_retry_seed(const char* id)
{
    uint32_t hash = _FNV_BASIS;
    if (id != NULL) {
        for (const char* c = id; *c != '\0'; ++c) {
            hash = (hash ^ (unsigned char)*c) * _FNV_PRIME;
        }
    }
    return hash;
}

uint32_t ebisu_retry_spread(uint32_t wait, uint32_t seed)
{
    /* The wait mixed in, so that the share changes from one wait to the next.
     * wait / 2 + 1 is never 0, and the result never past wait. */
    uint32_t hash = (seed ^ wait) * _FNV_PRIME;
    return wait - wait / 2 + hash % (wait / 2 + 1);
}

uint32_t ebisu_retry_wait(uint32_t first, uint32_t max, uint32_t n, uint32_t seed)
{
    uint32_t doublings = n > 1 ? n - 1 : 0;
    /* first doubled that many times is past max exactly when first is past
     * max >> doublings, and 32 doublings take any first but 0 past every max:
     * nothing overflows, and no loop. */
    uint32_t wait;
    if (first == 0) {
        wait = 0;
    } else if (doublings >= 32 || first > max >> doublings) {
        wait = max;
    } else {
        wait = first << doublings;
    }
    return ebisu_retry_spread(wait, seed);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
