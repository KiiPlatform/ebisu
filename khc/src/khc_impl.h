#ifndef __khc_impl__
#define __khc_impl__
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stdio.h>

#include "khc.h"

#ifdef EBISU_NO_LOG
/* Built without logging, nothing is logged, and the arguments aren't
   evaluated. */
#define _khc_log(khc, level, ...) ((void)0)
#define _khc_log_header(khc, direction, line, line_length) ((void)0)
#define _khc_log_lines(khc, direction, lines) ((void)0)
#else
/* Logs a message with the logger set by khc_set_logger(), if any. */
#define _khc_log(khc, level, ...) ebisu_log((khc)->_logger, (level), __VA_ARGS__)

/* Logs a line of a request's head, direction '>', or of a response's, '<', at
   EBISU_LOG_DEBUG, as it is. */
#define _khc_log_header(khc, direction, line, line_length) \
    _khc_log((khc), EBISU_LOG_DEBUG, "%c %.*s", (direction), (int)(line_length), (line))

/* Logs each of lines, each ending with CRLF, up to an empty one or the end, as
   _khc_log_header() does. */
void _khc_log_lines(khc* khc, char direction, const char* lines);

/* The name of a khc_code, such as "KHC_ERR_SOCK_CONNECT". */
const char* _khc_code_name(khc_code code);
#endif

int _is_chunked_encoding(const char* header, size_t header_length);

int _extract_content_length(const char* header, size_t header_length, size_t* out_content_length);

/* Returns 1 and the size if buff starts with a complete chunk size line, 0 if
 * the line has no CRLF yet, and -1 if it is not a valid size or the size is
 * more than a size_t holds. */
int _read_chunk_size(const char* buff, size_t buff_size, size_t* out_chunk_size);

int _is_header_present(const char* header, const char* buff, size_t buff_size);

/* Whether a line, without its CRLF, is an HTTP/1.x status line (RFC 9112,
 * section 4): "HTTP/1." and a digit, a space, three digits, and nothing more,
 * or a space and a reason phrase. */
bool _is_status_line(const char* line, size_t line_length);

/* Whether a line, without its CRLF, is a header (RFC 9110, section 5.1): a
 * name of token characters, a colon right after it, and a value. */
bool _is_header_line(const char* line, size_t line_length);

/* Whether the start of a header too long for the buffer could be one: a name,
 * its colon and a value so far, with a CR that may start the CRLF last. */
bool _is_header_start(const char* data, size_t data_length);

/* Whether bytes are allowed in a header's value or a reason phrase: HTAB, SP,
 * visible ASCII and bytes from 0x80 up. NUL, CR, LF, other control characters
 * and DEL are not. */
bool _is_field_text(const char* text, size_t text_length);

#ifdef __cplusplus
}
#endif
#endif // __khc_impl__
