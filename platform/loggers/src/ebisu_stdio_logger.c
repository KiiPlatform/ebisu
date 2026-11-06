#include "ebisu_loggers.h"
#include "ebisu_loggers_format.h"

#include <ebisu_object.h>

#include <stdio.h>
#include <string.h>

/* The line is formatted first, so that one fputs() writes it: each call to
   the stream's functions holds the stream's lock, on POSIX and on Windows, but
   two would let another task's line in between. */
static void stdio_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_stdio_logger_t* logger = EBISU_CONTAINER_OF(self, ebisu_stdio_logger_t, base);
    /* The line, newline included, and its NUL. */
    char line[EBISU_LOGGER_LINE_MAX + 1];
    size_t length;
    if (_EBISU_LOGGERS_DROPS(logger->_level, level)) {
        return;
    }
    length = strlen(_ebisu_loggers_level_names[level]);
    memcpy(line, _ebisu_loggers_level_names[level], length);
    memcpy(line + length, ": ", 2);
    /* Room for the newline. */
    length = _ebisu_loggers_format(line, sizeof(line) - 1, length + 2, fmt, args);
    line[length] = '\n';
    line[length + 1] = '\0';
    fputs(line, logger->_file);
    fflush(logger->_file);
}

static const ebisu_logger_ops_t stdio_logger_ops = {
    stdio_log
};

void ebisu_stdio_logger_init(
        ebisu_stdio_logger_t* logger,
        FILE* file,
        ebisu_log_level_t level)
{
    logger->base.ops = &stdio_logger_ops;
    logger->_file = file;
    logger->_level = level;
}
