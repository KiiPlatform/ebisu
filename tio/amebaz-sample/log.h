#ifndef LOG_H
#define LOG_H

/* Before the scheduler starts. */
void log_init(void);

/* Writes a line to the SDK's console, after the seconds since the start:
   whole, however many tasks write at once. Not from an interrupt handler. */
void sample_log(const char* format, ...) __attribute__((format(__printf__, 1, 2)));

/* Writes a line as sample_log() does, and stops: for what the sample can't go
   on without. */
void sample_fatal(const char* format, ...)
    __attribute__((noreturn, format(__printf__, 1, 2)));

/* A logger for ebisu's ports, which writes as sample_log() does. */
struct ebisu_logger_t* sample_logger(void);

#endif /* LOG_H */
