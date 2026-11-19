/**
 * @file ebisu_log.h
 * @brief Where a port's diagnostics go.
 */
#ifndef _EBISU_LOG
#define _EBISU_LOG

#include "ebisu_system.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what the shared object exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief How much a message matters, most first.
 */
typedef enum ebisu_log_level_t {
    EBISU_LOG_ERROR, /**< \brief An operation failed. */
    EBISU_LOG_WARNING, /**< \brief Something went wrong, but the operation went on. */
    EBISU_LOG_INFO, /**< \brief Worth knowing in normal operation. */
    EBISU_LOG_DEBUG /**< \brief Detail for finding a fault. */
} ebisu_log_level_t;

/** \brief A logger. See struct ebisu_logger_t. */
typedef struct ebisu_logger_t ebisu_logger_t;

/** \brief What a logger does. */
typedef struct ebisu_logger_ops_t {
    /**
     * \brief Logs a message.
     *
     * It may be called from more than one task at once.
     *
     * \param [in] self The logger.
     * \param [in] level How much the message matters.
     * \param [in] fmt The message, as a printf format, with no newline at the
     * end.
     * \param [in] args The format's arguments.
     */
    void (*log)(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt, va_list args);
} ebisu_logger_ops_t;

/** \brief A logger: where diagnostics go, such as stderr, syslog or the
 * device's own log.
 *
 * A port that takes one sends its diagnostics there. Embed it in a struct of
 * the application's if it needs state, as ebisu_object.h shows. platform's
 * ebisu_loggers.h has loggers to a stdio stream, to syslog and to a syslog
 * server, and ebisu_log_fanout_t gives each message to several.
 */
struct ebisu_logger_t {
    const ebisu_logger_ops_t* ops; /**< \brief What the logger does. */
};

/**
 * \brief Logs a message, with the logger's log.
 *
 * For an application to log messages of its own where a port logs its
 * diagnostics.
 *
 * \param [in] logger The logger, or NULL, which drops the message.
 * \param [in] level How much the message matters.
 * \param [in] fmt The message, as a printf format, with no newline at the end,
 * followed by its arguments.
 */
void ebisu_log(ebisu_logger_t* logger, ebisu_log_level_t level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    /* __printf__, which a C library or an SDK that defines printf as a macro,
       one that wraps it, leaves as it is. */
    __attribute__((format(__printf__, 3, 4)))
#endif
    ;

/**
 * \brief A logger that gives each message to several others.
 *
 * Each of them is given every message, in the order of the array, and decides
 * what to do with it, as platform's loggers do with a level of their own. So
 * one logger set on a port can send its diagnostics to, say, stderr and syslog
 * at once. It may be called from more than one task at once if each of them
 * may. It depends on nothing of the system's, so it serves an embedded device
 * too.
 *
 * Initialize with ebisu_log_fanout_init().
 */
typedef struct ebisu_log_fanout_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    ebisu_logger_t* const* _loggers; /**< \private */
    size_t _count; /**< \private */
} ebisu_log_fanout_t;

/**
 * \brief Initializes a logger that gives each message to the loggers of an
 * array, and makes base a logger of it.
 *
 * \param [out] fanout The logger.
 * \param [in] loggers The loggers, of which NULL ones are skipped. The array
 * and each logger in it have to outlive fanout, and the array can't change
 * while fanout may be logging.
 * \param [in] count How many there are.
 */
void ebisu_log_fanout_init(
        ebisu_log_fanout_t* fanout,
        ebisu_logger_t* const* loggers,
        size_t count);

/**
 * \brief Reads a level's name, as a configuration has it.
 *
 * "error", "warn" or "warning", "info", "debug", and "trace", which is
 * debug's, in any case, and nothing else: no spaces.
 *
 * \param [in] name The name, or NULL, which isn't one.
 * \param [out] out The level, left as it was if the name isn't one.
 * \return Whether it is one.
 */
bool ebisu_log_level_parse(const char* name, ebisu_log_level_t* out);

/**
 * \brief A level's name: "error", "warning", "info" or "debug", as
 * ebisu_log_level_parse() reads it.
 *
 * \param [in] level The level.
 * \return Its name, or "unknown" for a value that is none of them.
 */
const char* ebisu_log_level_name(ebisu_log_level_t level);

/**
 * \brief A logger that gives another the messages of a level that can change
 * while it logs.
 *
 * A message less important than the level is dropped before the other logger
 * is given it, so it isn't formatted at all. Its level can be set from any
 * task while others log through it, as from a configuration a server sends:
 * under the lock it is given, which each message takes for as long as it
 * reads the level, and no longer. So a change is for the messages that read
 * the level after it: one that read it before may still reach the other
 * logger after the change returns. It may be called from more than one task
 * at once if the other logger may.
 *
 * Initialize with ebisu_log_level_filter_init().
 */
typedef struct ebisu_log_level_filter_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    ebisu_logger_t* _target; /**< \private */
    ebisu_lock_t* _lock; /**< \private */
    ebisu_log_level_t _level; /**< \private */
} ebisu_log_level_filter_t;

/**
 * \brief Initializes a logger that gives target the messages of a level that
 * can change, and makes base a logger of it.
 *
 * \param [out] filter The logger.
 * \param [in] target The logger given the messages, which has to outlive
 * filter.
 * \param [in] level The least important level given to it: EBISU_LOG_WARNING
 * gives errors and warnings.
 * \param [in] lock The lock the level is read and set under, which has to
 * outlive filter, or NULL for none, where its level is set only while nothing
 * logs through it.
 */
void ebisu_log_level_filter_init(
        ebisu_log_level_filter_t* filter,
        ebisu_logger_t* target,
        ebisu_log_level_t level,
        ebisu_lock_t* lock);

/**
 * \brief Sets the least important level the filter gives its logger.
 *
 * With a lock, from any task, while others log through it.
 *
 * \param [in,out] filter The logger.
 * \param [in] level The level.
 */
void ebisu_log_level_filter_set_level(ebisu_log_level_filter_t* filter,
        ebisu_log_level_t level);

/**
 * \brief The least important level the filter gives its logger.
 *
 * \param [in] filter The logger.
 * \return The level.
 */
ebisu_log_level_t ebisu_log_level_filter_level(ebisu_log_level_filter_t* filter);

/**
 * \brief A place for a logger that can be filled, changed or emptied while
 * other tasks log through it.
 *
 * It gives each message to the logger it holds, and drops it while it holds
 * none. It takes the lock it is given for each message, while it gives it on,
 * and to change the logger, so that a change returns once no message is going
 * to the logger it held, which may then be closed: a logger to a syslog server
 * is changed for one to another server, or one with another host name, by
 * initializing the new one first, putting it in the place, and closing the old
 * one. It may be called from more than one task at once, which take their
 * turns at the lock, if the logger it holds may.
 *
 * The logger it holds mustn't log through it, as the lock is held while it
 * logs.
 *
 * Initialize with ebisu_log_slot_init().
 */
typedef struct ebisu_log_slot_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    ebisu_logger_t* _target; /**< \private */
    ebisu_lock_t* _lock; /**< \private */
} ebisu_log_slot_t;

/**
 * \brief Initializes an empty place for a logger, and makes base a logger of
 * it.
 *
 * \param [out] slot The logger.
 * \param [in] lock The lock each message and each change take, which has to
 * outlive slot.
 */
void ebisu_log_slot_init(ebisu_log_slot_t* slot, ebisu_lock_t* lock);

/**
 * \brief Puts a logger in the place, or empties it.
 *
 * From any task, while others log through it. It returns once no message is
 * going to the logger the place held: the caller may close it then.
 *
 * \param [in,out] slot The place.
 * \param [in] target The logger, which has to stay usable until it is taken
 * out of the place, or NULL to empty it.
 * \return The logger the place held, or NULL.
 */
ebisu_logger_t* ebisu_log_slot_set(ebisu_log_slot_t* slot, ebisu_logger_t* target);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_LOG */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
