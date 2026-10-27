#ifndef __khc_http_date__
#define __khc_http_date__

#include "khc.h"

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
