#include "ebisu_loggers.h"
#include "ebisu_loggers_format.h"

#include <ebisu_object.h>

#include <syslog.h>

/* syslog.h's own, though the values are RFC 5424's everywhere. */
static const int priorities[] = { LOG_ERR, LOG_WARNING, LOG_INFO, LOG_DEBUG };

/* vsyslog() would need no buffer, but isn't POSIX. */
static void syslog_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_syslog_logger_t* logger = EBISU_CONTAINER_OF(self, ebisu_syslog_logger_t, base);
    char message[EBISU_LOGGER_LINE_MAX + 1];
    if (_EBISU_LOGGERS_DROPS(logger->_level, level)) {
        return;
    }
    _ebisu_loggers_format(message, sizeof(message), 0, fmt, args);
    syslog(logger->_facility | priorities[level], "%s", message);
}

static const ebisu_logger_ops_t syslog_logger_ops = {
    syslog_log
};

void ebisu_syslog_logger_init(
        ebisu_syslog_logger_t* logger,
        int facility,
        ebisu_log_level_t level)
{
    logger->base.ops = &syslog_logger_ops;
    logger->_facility = facility;
    logger->_level = level;
}
