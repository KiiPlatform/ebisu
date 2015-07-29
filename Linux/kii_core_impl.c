#include "kii_core_impl.h"

#include <stdio.h>
#include <stdarg.h>

void logger_cb(const char* format, ...)
{
    va_list list;
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
