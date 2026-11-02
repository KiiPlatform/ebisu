/**
 * @file ebisu_log.h
 * @brief Where a port's diagnostics go.
 */
#ifndef _EBISU_LOG
#define _EBISU_LOG

#include <stdarg.h>

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
 * the application's if it needs state, as ebisu_object.h shows.
 */
struct ebisu_logger_t {
    const ebisu_logger_ops_t* ops; /**< \brief What the logger does. */
};

#ifdef __cplusplus
}
#endif

#endif /* _EBISU_LOG */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
