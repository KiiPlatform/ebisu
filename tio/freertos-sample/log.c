#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "board.h"

static SemaphoreHandle_t log_lock;

void log_init(void)
{
    log_lock = xSemaphoreCreateMutex();
    configASSERT(log_lock != NULL);
}

void sample_log(const char* format, ...)
{
    char line[256];
    TickType_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    int length = snprintf(line, sizeof(line), "[%5lu.%03lu] ",
            (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
    va_list args;
    va_start(args, format);
    int message_length = vsnprintf(line + length, sizeof(line) - length, format, args);
    va_end(args);
    if (message_length > 0) {
        length += message_length;
    }
    /* Cut short, if longer than the line, to make room for the end of it. */
    if (length > (int)sizeof(line) - 3) {
        length = sizeof(line) - 3;
    }
    line[length++] = '\r';
    line[length++] = '\n';

    xSemaphoreTake(log_lock, portMAX_DELAY);
    board_write(line, length);
    xSemaphoreGive(log_lock);
}
