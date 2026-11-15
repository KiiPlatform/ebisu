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

static const char* const level_names[] = { "error", "warning", "info", "debug" };

/* Whether s is the name, in any case. */
static bool is_name(const char* s, const char* name)
{
    size_t i;
    for (i = 0; name[i] != '\0'; ++i) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != name[i]) {
            return false;
        }
    }
    return s[i] == '\0';
}

bool ebisu_log_level_parse(const char* name, ebisu_log_level_t* out)
{
    static const struct {
        const char* name;
        ebisu_log_level_t level;
    } names[] = {
        { "error", EBISU_LOG_ERROR },
        { "warn", EBISU_LOG_WARNING },
        { "warning", EBISU_LOG_WARNING },
        { "info", EBISU_LOG_INFO },
        { "debug", EBISU_LOG_DEBUG },
        { "trace", EBISU_LOG_DEBUG }
    };
    size_t i;
    if (name == NULL) {
        return false;
    }
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (is_name(name, names[i].name)) {
            *out = names[i].level;
            return true;
        }
    }
    return false;
}

const char* ebisu_log_level_name(ebisu_log_level_t level)
{
    if ((int)level < 0 || (size_t)level >= sizeof(level_names) / sizeof(level_names[0])) {
        return "unknown";
    }
    return level_names[level];
}

/* The filter's level, under its lock if it has one. */
static ebisu_log_level_t filter_level(ebisu_log_level_filter_t* filter)
{
    ebisu_log_level_t level;
    if (filter->_lock == NULL) {
        return filter->_level;
    }
    filter->_lock->ops->lock(filter->_lock);
    level = filter->_level;
    filter->_lock->ops->unlock(filter->_lock);
    return level;
}

/* The lock is let go of before the message goes on, which a slow logger would
   otherwise hold every task's messages and every change of level up for. */
static void filter_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_log_level_filter_t* filter = EBISU_CONTAINER_OF(self, ebisu_log_level_filter_t, base);
    if (level > filter_level(filter)) {
        return;
    }
    filter->_target->ops->log(filter->_target, level, fmt, args);
}

static const ebisu_logger_ops_t filter_ops = {
    filter_log
};

void ebisu_log_level_filter_init(
        ebisu_log_level_filter_t* filter,
        ebisu_logger_t* target,
        ebisu_log_level_t level,
        ebisu_lock_t* lock)
{
    filter->base.ops = &filter_ops;
    filter->_target = target;
    filter->_lock = lock;
    filter->_level = level;
}

void ebisu_log_level_filter_set_level(ebisu_log_level_filter_t* filter,
        ebisu_log_level_t level)
{
    if (filter->_lock == NULL) {
        filter->_level = level;
        return;
    }
    filter->_lock->ops->lock(filter->_lock);
    filter->_level = level;
    filter->_lock->ops->unlock(filter->_lock);
}

ebisu_log_level_t ebisu_log_level_filter_level(ebisu_log_level_filter_t* filter)
{
    return filter_level(filter);
}

/* Under the lock while the message goes on, so that a change waits for it. */
static void slot_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_log_slot_t* slot = EBISU_CONTAINER_OF(self, ebisu_log_slot_t, base);
    slot->_lock->ops->lock(slot->_lock);
    if (slot->_target != NULL) {
        slot->_target->ops->log(slot->_target, level, fmt, args);
    }
    slot->_lock->ops->unlock(slot->_lock);
}

static const ebisu_logger_ops_t slot_ops = {
    slot_log
};

void ebisu_log_slot_init(ebisu_log_slot_t* slot, ebisu_lock_t* lock)
{
    slot->base.ops = &slot_ops;
    slot->_target = NULL;
    slot->_lock = lock;
}

ebisu_logger_t* ebisu_log_slot_set(ebisu_log_slot_t* slot, ebisu_logger_t* target)
{
    ebisu_logger_t* old;
    slot->_lock->ops->lock(slot->_lock);
    old = slot->_target;
    slot->_target = target;
    slot->_lock->ops->unlock(slot->_lock);
    return old;
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
