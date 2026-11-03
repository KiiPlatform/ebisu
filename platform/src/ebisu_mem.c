#include "ebisu_mem.h"

#include <string.h>

/* memset(), called through a pointer the compiler has to read when the call is
   made, and which might by then point at anything: so it can't know the call
   is memset()'s, nor take it for a store nothing reads. */
static void* (*const volatile _memset)(void*, int, size_t) = memset;

void ebisu_secure_zero(void* p, size_t n)
{
    if (n > 0) {
        _memset(p, 0, n);
    }
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
