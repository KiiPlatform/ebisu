/**
 * @file ebisu_loggers.h
 * @brief Loggers to a stdio stream, to syslog, and to a syslog server over UDP.
 */
#ifndef EBISU_LOGGERS_H
#define EBISU_LOGGERS_H

#include <ebisu_log.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* The library is compiled with hidden visibility, so what this header declares
   is exactly what a shared object built with it exports. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility push(default)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief The longest line, in bytes, these loggers write or send: a line with
 * its newline, a message to syslog or a datagram.
 *
 * A message too long for one is cut short, and ends with "...".
 */
#define EBISU_LOGGER_LINE_MAX 1024

/**
 * \brief A logger to a stdio stream, such as stderr or a file.
 *
 * Each message is a line of its level and itself, as in "error: failed to
 * connect to example.com.". A message less important than the logger's level
 * is dropped. The line is written with one call, which the C library of a POSIX
 * system or of Windows doesn't let another task's writes to the stream come
 * into, and flushed, so that it isn't lost if the program ends. So it may be
 * called from more than one task at once.
 *
 * Initialize with ebisu_stdio_logger_init().
 */
typedef struct ebisu_stdio_logger_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    FILE* _file; /**< \private */
    ebisu_log_level_t _level; /**< \private */
} ebisu_stdio_logger_t;

/**
 * \brief Initializes a logger to a stdio stream, and makes base a logger of it.
 *
 * \param [out] logger The logger.
 * \param [in] file The stream, which has to outlive the logger, such as stderr.
 * \param [in] level The least important level written: EBISU_LOG_WARNING
 * writes errors and warnings.
 */
void ebisu_stdio_logger_init(
        ebisu_stdio_logger_t* logger,
        FILE* file,
        ebisu_log_level_t level);

/**
 * \brief Room for the path of a file ebisu_file_logger_t writes to, with its
 * NUL.
 */
#define EBISU_FILE_LOGGER_PATH_MAX 256

/** \brief Why a logger couldn't be initialized, for
 * ebisu_file_logger_init() and ebisu_syslog_udp_logger_init(). */
typedef enum ebisu_logger_init_error_t {
    /** \brief It was. */
    EBISU_LOGGER_INIT_ERROR_NONE,
    /** \brief An argument is none it takes: a path that is empty or too
     * long, a facility out of range, a HOSTNAME or an application's name
     * RFC 5424 can't carry. */
    EBISU_LOGGER_INIT_ERROR_ARGUMENT,
    /** \brief The server's host couldn't be looked up. */
    EBISU_LOGGER_INIT_ERROR_LOOKUP,
    /** \brief The system didn't open the file, or a socket to any of the
     * server's addresses. */
    EBISU_LOGGER_INIT_ERROR_OPEN
} ebisu_logger_init_error_t;

/**
 * \brief A logger to a file it keeps under a size, on POSIX and on Windows.
 *
 * Each message is a line of its level and itself, as ebisu_stdio_logger_t
 * writes it, after the system's time of day if
 * ebisu_file_logger_set_timestamps() asks for it. A message less important
 * than the logger's level is dropped.
 * When a line would take the file past its size, the file is first renamed
 * to its path with ".1" after it, which replaces the one there, and a new one
 * is started: what the device keeps of its log is at most twice the size, and
 * the last of it is in the file. A line longer than the size goes in a file of
 * its own.
 *
 * The line is formatted before the lock it is given is taken, and written,
 * flushed and counted under it, as the file is renamed, so that it may be
 * called from more than one task at once, which wait only for each other's
 * writes. Nothing is lost but for the system failing to write: a file that
 * can't be renamed, as one something holds open on Windows, is written past
 * its size, and renamed once another size's worth has come, the backup kept
 * meanwhile; and one that can't be opened is tried again with the next line.
 *
 * Initialize with ebisu_file_logger_init(), and close with
 * ebisu_file_logger_close().
 */
typedef struct ebisu_file_logger_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    FILE* _file; /**< \private */
    /** \private How much of the file there is. */
    size_t _size;
    size_t _max_bytes; /**< \private */
    ebisu_log_level_t _level; /**< \private */
    ebisu_lock_t* _lock; /**< \private */
    /** \private Whether each line starts with the time of day. */
    bool _timestamps;
    /** \private The path, or empty once closed. */
    char _path[EBISU_FILE_LOGGER_PATH_MAX];
} ebisu_file_logger_t;

/**
 * \brief Initializes a logger to a file it keeps under a size, and makes base
 * a logger of it.
 *
 * It opens the file to add to it, or makes it, and counts what it has.
 *
 * \param [out] logger The logger.
 * \param [in] path The file's path, shorter than EBISU_FILE_LOGGER_PATH_MAX
 * less 2, for ".1".
 * \param [in] max_bytes How large the file may grow, or 0 for no bound.
 * \param [in] level The least important level written: EBISU_LOG_WARNING
 * writes errors and warnings.
 * \param [in] lock The lock each line's write takes, which has to outlive the
 * logger, or NULL for a logger one task logs to at a time.
 * \param [out] out_error Why it failed, or EBISU_LOGGER_INIT_ERROR_NONE if it
 * didn't: EBISU_LOGGER_INIT_ERROR_ARGUMENT for a path empty or too long, when
 * the logger drops every message until closed, or
 * EBISU_LOGGER_INIT_ERROR_OPEN for a file that can't be opened, when it tries
 * again with each message. NULL for none.
 * \return true if the file is open.
 */
bool ebisu_file_logger_init(
        ebisu_file_logger_t* logger,
        const char* path,
        size_t max_bytes,
        ebisu_log_level_t level,
        ebisu_lock_t* lock,
        ebisu_logger_init_error_t* out_error);

/**
 * \brief Sets whether each line starts with the time it is written.
 *
 * The system's time of day, in UTC to the millisecond, as RFC 3339 has it, and
 * a space, as in "2026-11-15T07:46:05.123Z error: failed to connect.", or "-"
 * and a space if the system has no time. It is taken as the line is written,
 * under the logger's lock, so that the times of a file's lines are in order,
 * unless the system's clock is set back. The line, with it, is at most
 * EBISU_LOGGER_LINE_MAX bytes still. Without it, as the logger starts, a line
 * is as ebisu_stdio_logger_t's.
 *
 * Only before the logger logs, from the task that initialized it, as for its
 * other settings.
 *
 * \param [in,out] logger The logger.
 * \param [in] timestamps Whether each line starts with the time.
 */
void ebisu_file_logger_set_timestamps(ebisu_file_logger_t* logger, bool timestamps);

/**
 * \brief Closes a logger to a file, which drops every message after.
 *
 * Nothing may be logging with it when it is called.
 *
 * \param [in,out] logger The logger, initialized whether or not that
 * succeeded.
 */
void ebisu_file_logger_close(ebisu_file_logger_t* logger);

#ifndef _WIN32
/**
 * \brief A logger to the system's syslog, on POSIX.
 *
 * Each message goes to syslog() with the logger's facility and the severity of
 * its level: LOG_ERR, LOG_WARNING, LOG_INFO or LOG_DEBUG. A message less
 * important than the logger's level is dropped. openlog(), which names the
 * program in each message and sets the facility by default, is the
 * application's to call, if it wants to. syslog() may be called from more than
 * one task at once, and so may the logger.
 *
 * Initialize with ebisu_syslog_logger_init().
 */
typedef struct ebisu_syslog_logger_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    int _facility; /**< \private */
    ebisu_log_level_t _level; /**< \private */
} ebisu_syslog_logger_t;

/**
 * \brief Initializes a logger to the system's syslog, and makes base a logger
 * of it.
 *
 * \param [out] logger The logger.
 * \param [in] facility A facility of syslog.h, such as LOG_DAEMON or LOG_LOCAL0,
 * or 0 for the one openlog() set, LOG_USER without it.
 * \param [in] level The least important level sent: EBISU_LOG_WARNING sends
 * errors and warnings.
 */
void ebisu_syslog_logger_init(
        ebisu_syslog_logger_t* logger,
        int facility,
        ebisu_log_level_t level);
#endif /* _WIN32 */

/**
 * \brief The longest application name a syslog message carries, as RFC 5424
 * has it.
 */
#define EBISU_SYSLOG_APP_NAME_MAX 48

/**
 * \brief The longest host name a syslog message carries, as RFC 5424 has it.
 */
#define EBISU_SYSLOG_HOSTNAME_MAX 255

/**
 * \brief A logger to a syslog server, over UDP, on POSIX and on Windows.
 *
 * Each message is a datagram of its own, formatted as RFC 5424 says and sent
 * as RFC 5426 does:
 *
 *     <PRI>1 TIMESTAMP HOSTNAME APP-NAME PROCID - - MSG
 *
 * PRI is the logger's facility and the severity of the message's level, as
 * ebisu_syslog_logger_t has them, TIMESTAMP the system's time of day, in UTC to
 * the millisecond, HOSTNAME the name the logger is given, or the host's, as
 * gethostname() gives it, APP-NAME the application's name, and PROCID the
 * process's identifier. A
 * message has no identifier and no structured data. The datagram is at most
 * EBISU_LOGGER_LINE_MAX bytes. A message less important than the logger's level
 * is dropped.
 *
 * The server's address is looked up when the logger is initialized, and not
 * for each message. Sending never waits: a datagram the socket can't take at
 * once is dropped, as is one the network loses, as with any datagram. So the
 * logger may be called from more than one task at once, and from one that
 * mustn't wait, such as the MQTT task.
 *
 * Initialize with ebisu_syslog_udp_logger_init(), and close with
 * ebisu_syslog_udp_logger_close().
 */
typedef struct ebisu_syslog_udp_logger_t {
    ebisu_logger_t base; /**< \brief What ebisu is given. */
    /** \private The socket: a file descriptor on POSIX, a SOCKET on Windows. */
    uintptr_t _socket;
    /** \private Whether _socket is open. */
    bool _open;
    int _facility; /**< \private */
    ebisu_log_level_t _level; /**< \private */
    /** \private " HOSTNAME APP-NAME PROCID - - ", with its NUL. */
    char _fields[1 + EBISU_SYSLOG_HOSTNAME_MAX + 1 + EBISU_SYSLOG_APP_NAME_MAX + 1 + 20 + 7];
} ebisu_syslog_udp_logger_t;

/**
 * \brief Initializes a logger to a syslog server over UDP, and makes base a
 * logger of it.
 *
 * It looks the server up, which may take as long as the system's resolver
 * does, and opens a socket to it. If it can't, it returns false, and the logger
 * drops every message until closed.
 *
 * \param [out] logger The logger.
 * \param [in] host The server's host name or address, IPv4 or IPv6.
 * \param [in] port Its port, 514 for syslog's.
 * \param [in] hostname The HOSTNAME the server is sent, which it tells hosts
 * apart by, 1 to EBISU_SYSLOG_HOSTNAME_MAX printable ASCII characters without
 * spaces, such as a device's serial number where every device has the same
 * host name, or NULL for the host's name, as gethostname() gives it, or none if
 * it gives none RFC 5424 can carry.
 * \param [in] app_name The application's name, 1 to EBISU_SYSLOG_APP_NAME_MAX
 * printable ASCII characters without spaces, or NULL for none.
 * \param [in] facility The facility, as RFC 5424 numbers them, 0 to 23: 1 is
 * user-level messages, 3 system daemons, and 16 to 23 local use 0 to 7.
 * \param [in] level The least important level sent: EBISU_LOG_WARNING sends
 * errors and warnings.
 * \param [out] out_error Why it failed, or EBISU_LOGGER_INIT_ERROR_NONE if it
 * didn't: EBISU_LOGGER_INIT_ERROR_ARGUMENT for an argument that isn't valid,
 * EBISU_LOGGER_INIT_ERROR_LOOKUP for a host that can't be looked up, or
 * EBISU_LOGGER_INIT_ERROR_OPEN if no socket opens to it. NULL for none.
 * \return true if the socket is open.
 */
bool ebisu_syslog_udp_logger_init(
        ebisu_syslog_udp_logger_t* logger,
        const char* host,
        uint16_t port,
        const char* hostname,
        const char* app_name,
        int facility,
        ebisu_log_level_t level,
        ebisu_logger_init_error_t* out_error);

/**
 * \brief Closes a logger to a syslog server, which drops every message after.
 *
 * Nothing may be logging with it when it is called.
 *
 * \param [in,out] logger The logger, initialized whether or not that succeeded.
 */
void ebisu_syslog_udp_logger_close(ebisu_syslog_udp_logger_t* logger);

#ifdef __cplusplus
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC visibility pop
#endif

#endif /* EBISU_LOGGERS_H */
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
