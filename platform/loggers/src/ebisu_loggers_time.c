#include "ebisu_loggers_format.h"

#include <ebisu_time.h>

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

/* The system's time of day, as RFC 5424 has a timestamp: UTC, to the
   millisecond. "-", RFC 5424's for a value unknown, if it has none. */
void _ebisu_loggers_timestamp(char* out, size_t size)
{
    int64_t seconds;
    long milliseconds;
    ebisu_civil_time_t civil;
#ifdef _WIN32
    FILETIME now;
    ULARGE_INTEGER intervals;
    GetSystemTimeAsFileTime(&now);
    /* 100-nanosecond intervals since 1601-01-01T00:00:00Z. */
    intervals.LowPart = now.dwLowDateTime;
    intervals.HighPart = now.dwHighDateTime;
    seconds = (int64_t)(intervals.QuadPart / 10000000u) - 11644473600;
    milliseconds = (long)(intervals.QuadPart / 10000u % 1000u);
#else
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        snprintf(out, size, "-");
        return;
    }
    seconds = (int64_t)now.tv_sec;
    milliseconds = now.tv_nsec / 1000000;
#endif
    if (!ebisu_time_to_civil(seconds, &civil)) {
        snprintf(out, size, "-");
        return;
    }
    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ", civil.year, civil.month,
            civil.day, civil.hour, civil.minute, civil.second, milliseconds);
}
/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
