#include "ebisu_loggers.h"
#include "ebisu_loggers_format.h"

#include <ebisu_object.h>

#include <stdio.h>
#include <string.h>

/* The file at the logger's path, to add to, and how much it has. Binary, so
   that what is counted is what is written, a newline too, on Windows. */
static void open_file(ebisu_file_logger_t* logger, const char* mode)
{
    long size;
    logger->_file = fopen(logger->_path, mode);
    logger->_size = 0;
    if (logger->_file == NULL) {
        return;
    }
    if (fseek(logger->_file, 0, SEEK_END) == 0 && (size = ftell(logger->_file)) > 0) {
        logger->_size = (size_t)size;
    }
}

/* The file renamed to its backup, which it replaces, and a new one started.
   One that can't be renamed is added to again, past its size, rather than
   lose what comes. */
static void rotate(ebisu_file_logger_t* logger)
{
    char backup[EBISU_FILE_LOGGER_PATH_MAX + 2];
    snprintf(backup, sizeof(backup), "%s.1", logger->_path);
    fclose(logger->_file);
    logger->_file = NULL;
    /* Windows' rename() doesn't replace a file. */
    remove(backup);
    if (rename(logger->_path, backup) != 0) {
        open_file(logger, "ab");
        return;
    }
    open_file(logger, "wb");
}

/* Room before a line for its timestamp, 24 bytes, and the space after it. */
#define TIMESTAMP_ROOM 25

/* The time of day before the line at start in line, under the lock, and where
   the line starts with it. "-" if the system has no time, or gave more than
   there is room for. */
static size_t put_timestamp(char* line, size_t start)
{
    /* Room for any int and long the format could be given, as GCC checks for,
       though a timestamp takes 24 bytes. */
    char timestamp[96];
    size_t length;
    _ebisu_loggers_timestamp(timestamp, sizeof(timestamp));
    length = strlen(timestamp);
    if (length + 1 > start) {
        timestamp[0] = '-';
        length = 1;
    }
    memcpy(line + start - length - 1, timestamp, length);
    line[start - 1] = ' ';
    return start - length - 1;
}

/* The line is formatted before the lock is taken, as in the stdio logger, so
   that the tasks wait only for each other's writes. With timestamps, the room
   for one is kept before it, and filled under the lock, so that the lines'
   times are in the order the lines are. */
static void file_log(
        ebisu_logger_t* self,
        ebisu_log_level_t level,
        const char* fmt,
        va_list args)
{
    ebisu_file_logger_t* logger = EBISU_CONTAINER_OF(self, ebisu_file_logger_t, base);
    /* The line, newline included, and its NUL. */
    char line[EBISU_LOGGER_LINE_MAX + 1];
    size_t start = logger->_timestamps ? TIMESTAMP_ROOM : 0;
    size_t length;
    if (_EBISU_LOGGERS_DROPS(logger->_level, level)) {
        return;
    }
    length = strlen(_ebisu_loggers_level_names[level]);
    memcpy(line + start, _ebisu_loggers_level_names[level], length);
    memcpy(line + start + length, ": ", 2);
    /* Room for the newline. */
    length = _ebisu_loggers_format(line, sizeof(line) - 1, start + length + 2, fmt, args);
    line[length++] = '\n';
    line[length] = '\0';

    if (logger->_lock != NULL) {
        logger->_lock->ops->lock(logger->_lock);
    }
    if (start > 0) {
        start = put_timestamp(line, start);
    }
    length -= start;
    if (logger->_file == NULL && logger->_path[0] != '\0') {
        open_file(logger, "ab");
    }
    if (logger->_file != NULL && logger->_max_bytes > 0 && logger->_size > 0 &&
            logger->_size + length > logger->_max_bytes) {
        rotate(logger);
    }
    if (logger->_file != NULL) {
        size_t written = fwrite(line + start, 1, length, logger->_file);
        fflush(logger->_file);
        logger->_size += written;
    }
    if (logger->_lock != NULL) {
        logger->_lock->ops->unlock(logger->_lock);
    }
}

static const ebisu_logger_ops_t file_logger_ops = {
    file_log
};

bool ebisu_file_logger_init(
        ebisu_file_logger_t* logger,
        const char* path,
        size_t max_bytes,
        ebisu_log_level_t level,
        ebisu_lock_t* lock,
        ebisu_logger_init_error_t* out_error)
{
    ebisu_logger_init_error_t ignored;
    size_t length = path != NULL ? strlen(path) : EBISU_FILE_LOGGER_PATH_MAX;
    logger->base.ops = &file_logger_ops;
    logger->_file = NULL;
    logger->_size = 0;
    logger->_max_bytes = max_bytes;
    logger->_level = level;
    logger->_lock = lock;
    logger->_timestamps = false;
    logger->_path[0] = '\0';
    if (out_error == NULL) {
        out_error = &ignored;
    }
    /* Room for ".1" after it. */
    if (length == 0 || length + 2 >= sizeof(logger->_path)) {
        *out_error = EBISU_LOGGER_INIT_ERROR_ARGUMENT;
        return false;
    }
    memcpy(logger->_path, path, length + 1);
    open_file(logger, "ab");
    *out_error = logger->_file != NULL ? EBISU_LOGGER_INIT_ERROR_NONE :
        EBISU_LOGGER_INIT_ERROR_OPEN;
    return logger->_file != NULL;
}

void ebisu_file_logger_set_timestamps(ebisu_file_logger_t* logger, bool timestamps)
{
    logger->_timestamps = timestamps;
}

void ebisu_file_logger_close(ebisu_file_logger_t* logger)
{
    if (logger->_file != NULL) {
        fclose(logger->_file);
        logger->_file = NULL;
    }
    logger->_path[0] = '\0';
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
