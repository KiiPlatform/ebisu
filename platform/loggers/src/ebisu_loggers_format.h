#ifndef _EBISU_LOGGERS_FORMAT
#define _EBISU_LOGGERS_FORMAT

#include <ebisu_log.h>

#include <stdarg.h>
#include <stddef.h>

/* What the loggers write each level as, and its severity as RFC 5424 numbers
   them. */
extern const char* const _ebisu_loggers_level_names[];
extern const int _ebisu_loggers_severities[];

/* Whether a message at level is less important than the logger's least
   important, or isn't a level at all, and so is dropped. */
#define _EBISU_LOGGERS_DROPS(logger_level, level) \
    ((unsigned)(level) > (unsigned)(logger_level) || (unsigned)(level) > EBISU_LOG_DEBUG)

/* Formats the message after the used bytes of line, of size bytes, cutting it
   short with "..." if it doesn't fit. Returns the length of line, which is
   NUL-terminated. */
size_t _ebisu_loggers_format(
        char* line,
        size_t size,
        size_t used,
        const char* fmt,
        va_list args);

#endif /* _EBISU_LOGGERS_FORMAT */
