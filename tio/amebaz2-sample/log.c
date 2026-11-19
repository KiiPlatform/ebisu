#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include <ebisu_log.h>

static SemaphoreHandle_t log_lock;

void log_init(void)
{
    log_lock = xSemaphoreCreateMutex();
    configASSERT(log_lock != NULL);
}

/* The line, after the seconds since the start, with no more than fits. The
   SDK's printf writes to its console. */
static void write_line(const char* format, va_list args)
{
    char line[256];
    TickType_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    int length = snprintf(line, sizeof(line), "[%5lu.%03lu] ",
            (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
    if (length < 0 || length >= (int)sizeof(line)) {
        length = 0;
    }
    vsnprintf(line + length, sizeof(line) - length, format, args);

    xSemaphoreTake(log_lock, portMAX_DELAY);
    printf("%s\r\n", line);
    xSemaphoreGive(log_lock);
}

void sample_log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    write_line(format, args);
    va_end(args);
}

void sample_fatal(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    write_line(format, args);
    va_end(args);
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

static void logger_log(ebisu_logger_t* self, ebisu_log_level_t level, const char* fmt,
        va_list args)
{
    char message[200];
    (void)self;
    (void)level;
    vsnprintf(message, sizeof(message), fmt, args);
    sample_log("%s", message);
}

static const ebisu_logger_ops_t logger_ops = { logger_log };
static ebisu_logger_t logger = { &logger_ops };

ebisu_logger_t* sample_logger(void)
{
    return &logger;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
