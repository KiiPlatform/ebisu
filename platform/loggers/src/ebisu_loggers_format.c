#include "ebisu_loggers_format.h"

#include <stdio.h>
#include <string.h>

const char* const _ebisu_loggers_level_names[] = {
    "error",
    "warning",
    "info",
    "debug"
};

/* Error, warning, informational and debug. */
const int _ebisu_loggers_severities[] = { 3, 4, 6, 7 };

size_t _ebisu_loggers_format(
        char* line,
        size_t size,
        size_t used,
        const char* fmt,
        va_list args)
{
    static const char cut[] = "...";
    int written;
    if (used >= size) {
        used = size - 1;
        line[used] = '\0';
        return used;
    }
    written = vsnprintf(line + used, size - used, fmt, args);
    if (written < 0) {
        /* An encoding error: the message is left out, rather than half of it. */
        line[used] = '\0';
        return used;
    }
    if ((size_t)written < size - used) {
        return used + (size_t)written;
    }
    /* Cut short: vsnprintf wrote what fit, and its NUL at the end. */
    if (size - 1 - used >= sizeof(cut) - 1) {
        memcpy(line + size - sizeof(cut), cut, sizeof(cut) - 1);
    }
    return size - 1;
}
