/* jkii.c, with a strtod() that leaves errno as it was: as where errno.h isn't
 * the C library's own, and the errno jkii sees is not the one strtod() sets.
 * jkii tells a double out of range from what strtod() gives, and its double
 * tests run against this build too. */
#include <errno.h>
#include <stdlib.h>

static double strtod_without_errno(const char* s, char** end)
{
    int saved = errno;
    double value = strtod(s, end);
    errno = saved;
    return value;
}

#undef strtod
#define strtod strtod_without_errno
#include "jkii.c"
