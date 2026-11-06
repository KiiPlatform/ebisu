#include "ebisu_log.h"
#include "ebisu_object.h"

void ebisu_log(ebisu_logger_t* logger, ebisu_log_level_t level, const char* fmt, ...)
{
    va_list args;
    if (logger == NULL) {
        return;
    }
    va_start(args, fmt);
    logger->ops->log(logger, level, fmt, args);
    va_end(args);
}

/* Each logger reads the arguments from a copy of its own: one that read them
   from args would leave nothing for the next. */
static void fanout_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_log_fanout_t* fanout = EBISU_CONTAINER_OF(self, ebisu_log_fanout_t, base);
    size_t i;
    for (i = 0; i < fanout->_count; ++i) {
        ebisu_logger_t* logger = fanout->_loggers[i];
        va_list copy;
        if (logger == NULL) {
            continue;
        }
        va_copy(copy, args);
        logger->ops->log(logger, level, fmt, copy);
        va_end(copy);
    }
}

static const ebisu_logger_ops_t fanout_ops = {
    fanout_log
};

void ebisu_log_fanout_init(
        ebisu_log_fanout_t* fanout,
        ebisu_logger_t* const* loggers,
        size_t count)
{
    fanout->base.ops = &fanout_ops;
    fanout->_loggers = loggers;
    fanout->_count = count;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
