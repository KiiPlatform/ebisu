#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

#ifdef __cplusplus
extern "C"
{
#endif

kii_code_t _convert_code(khc_code khc_c);

void _reset_buff(kii_t* kii);

/* Resets khc for a new request, keeping the buffers set by
 * kii_set_stream_buff() and kii_set_resp_header_buff(). Use it rather than
 * khc_reset_except_cb(), which drops them. */
void _reset_khc(kii_t* kii);

void _req_headers_free_all(kii_t* kii);

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len);

/* Seconds since the epoch of a Date header line, as khc hands it to the header
 * callback, or -1 when the line is not a Date header in IMF-fixdate form. */
int64_t _parse_date(const char* header, size_t header_len);

extern const char _APP_KEY_HEADER[];
extern const char _CONTENT_LENGTH_ZERO[];

size_t _cb_read_buff(char *buffer, size_t size, void *userdata);
size_t _cb_write_buff(char *buffer, size_t size, void *userdata);

#ifdef __cplusplus
}
#endif
#endif
