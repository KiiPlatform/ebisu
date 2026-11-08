#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

kii_code_t _convert_code(khc_code khc_c);

#ifdef EBISU_NO_LOG
/* Built without logging, nothing is logged, and the arguments aren't
   evaluated. */
#define _kii_log(kii, level, ...) ((void)0)
#else
/* Logs a message with the logger set by kii_set_logger(), if any. */
#define _kii_log(kii, level, ...) ebisu_log((kii)->_logger, (level), __VA_ARGS__)
#endif

void _reset_buff(kii_t* kii);

/* Resets khc for a new request, keeping the buffers set by
 * kii_set_stream_buff() and kii_set_resp_header_buff(). Use it rather than
 * khc_reset_except_cb(), which drops them. */
void _reset_khc(kii_t* kii);

void _req_headers_free_all(kii_t* kii);

/* Clears the buffer set by kii_set_buff(), as kii->_rw_buff points at it, and
 * the one set by kii_set_stream_buff(), after a request whose body or response
 * held a secret: a password, a token or MQTT credentials. A response whose
 * status refused the request is kept, as it says why and holds none. khc
 * clears a stream buffer of its own as it frees it. */
void _kii_clear_buffers(kii_t* kii);

/* Gives the clock set with kii_set_clock(), if any, the date of the response
 * just read. */
void _kii_clock_record(kii_t* kii);

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len);

/* Appends to the request body being built in the buffer, of length len so far:
 * before, value escaped for a JSON string, and after. Returns the new length,
 * or -1 if that doesn't fit, or if len is -1 already, so that calls can be
 * chained and checked once. */
int _kii_body_append(
        kii_t* kii,
        int len,
        const char* before,
        const char* value,
        const char* after);

extern const char _APP_KEY_HEADER[];
extern const char _CONTENT_LENGTH_ZERO[];

size_t _cb_read_buff(char *buffer, size_t size, void *userdata);
size_t _cb_write_buff(char *buffer, size_t size, void *userdata);

#endif
