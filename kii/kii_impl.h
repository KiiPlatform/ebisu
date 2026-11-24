#ifndef KII_IMPL_H
#define KII_IMPL_H

#include "kii.h"
#include "khc.h"

kii_code_t _convert_code(khc_code_t khc_c);

#ifdef EBISU_NO_LOG
/* Built without logging, nothing is logged, and the arguments aren't
   evaluated. */
#define _kii_log(kii, level, ...) ((void)0)
#else
/* Logs a message with the logger set by kii_set_logger(), if any. */
#define _kii_log(kii, level, ...) ebisu_log((kii)->_logger, (level), __VA_ARGS__)
#endif

void _reset_buff(kii_t* kii);

/* Whether a field jkii_parse() was to copy was too long for where it goes, a
 * token for its array in kii_author_t: logged, with the size and the name of
 * what sets it, as nothing else says why the token wasn't taken. */
bool _kii_field_too_long(kii_t* kii, const jkii_field_t* field, const char* what,
        const char* size_name);
/* Whether a string field jkii found, without copying it, fits a buffer of size
 * bytes: for a response read through once before anything of it is kept, so
 * that one that can't be read whole changes nothing. Logged if it doesn't. */
bool _kii_field_fits(kii_t* kii, const jkii_field_t* field, size_t size, const char* what,
        const char* size_name);

/* Resets khc for a new request, keeping the buffers set by
 * kii_set_stream_buff() and kii_set_resp_header_buff(). Use it rather than
 * khc_reset_except_cb(), which drops them. */
void _reset_khc(kii_t* kii);

/* Readies the request's header buffer for its first header: the one
 * kii_set_req_header_buff() gave, or one of KII_REQ_HEADER_BUFF_SIZE bytes
 * allocated for the request. KII_ERR_ALLOCATION if it can't be. */
kii_code_t _req_headers_open(kii_t* kii);

/* Clears the request's headers, and frees the buffer allocated for them. */
void _req_headers_free_all(kii_t* kii);

/* Clears the buffer set by kii_set_buff(), as kii->_rw_buff points at it, and
 * the one set by kii_set_stream_buff(), after a request whose body or response
 * held a secret: a password, a token or MQTT credentials. A response whose
 * status refused the request is kept, as it says why and holds none. khc_t
 * clears a stream buffer of its own as it frees it. */
void _kii_clear_buffers(kii_t* kii);

/* Gives the clock set with kii_set_clock(), if any, the date of the response
 * just read. */
void _kii_clock_record(kii_t* kii);

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len);

/* Forgets what the last response's headers said: its ETag and whether one
 * was lost, whether its Content-Type is Kii's for an error, its X-Step-Count
 * and its X-Environment-Version. */
void _kii_clear_resp_headers(kii_t* kii);

/* Reads what kii keeps of one response header, given as khc gives it to
 * kii's header callback, without its CRLF. */
void _kii_read_resp_header(kii_t* kii, const char* header, size_t header_len);

/* Whether the last response may have had an ETag kii_get_etag() doesn't give:
   one too long for kii's buffer, or a header too long for the response header
   buffer, which khc skipped, with no ETag read. */
bool _kii_etag_unknown(kii_t* kii);

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
