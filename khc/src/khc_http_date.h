#ifndef KHC_HTTP_DATE_H
#define KHC_HTTP_DATE_H

#include "khc.h"

/* The latest date khc takes from a response, in years after the library's
 * source, EBISU_SOURCE_TIME: a build setting, -DEBISU_DATE_CEILING_YEARS=<n>
 * in CMake or the definition elsewhere. 0 takes any. */
#ifndef EBISU_DATE_CEILING_YEARS
#define EBISU_DATE_CEILING_YEARS 15
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* Seconds since the epoch of a Date header line, as khc hands it to the header
 * callback. -1 when the line is not a Date header, -2 when it is one whose value
 * is in none of RFC 9110's three forms, or earlier than floor. */
int64_t _parse_http_date(const char* header, size_t header_len, int64_t floor);

/* Keeps the Date of the response a header line belongs to. */
void _record_resp_date(khc* khc, const char* line, size_t line_len);

#ifdef __cplusplus
}
#endif
#endif
