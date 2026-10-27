#ifndef LOG_H
#define LOG_H

/* Before the scheduler starts. */
void log_init(void);

/* Writes a line to the console, after the seconds since the start: whole,
   however many tasks write at once. Not from an interrupt handler. */
void sample_log(const char* format, ...) __attribute__((format(printf, 1, 2)));

#endif /* LOG_H */
