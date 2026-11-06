/**
 * @file ebisu_log.h
 * @brief Where a port's diagnostics go.
 */
#ifndef _EBISU_LOG
#define _EBISU_LOG

#include <stdarg.h>
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
    __attribute__((format(printf, 3, 4)))
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

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* _EBISU_LOG */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
