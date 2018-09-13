#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

kii_code_t _convert_code(khc_code khc_c);

void _reset_buff(kii_t* kii);

void _req_headers_free_all(kii_t* kii);

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len);

extern const char _APP_KEY_HEADER[];
extern const char _CONTENT_LENGTH_ZERO[];

size_t _cb_read_buff(char *buffer, size_t size, size_t count, void *userdata);
size_t _cb_write_buff(char *buffer, size_t size, size_t count, void *userdata);

#endif