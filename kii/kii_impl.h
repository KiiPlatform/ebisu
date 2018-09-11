#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

int _kii_set_content_length(kii_t* kii, size_t content_length);

kii_code_t _convert_code(khc_code khc_c);

void _reset_buff(kii_t* kii);

int _parse_etag(char* header, size_t header_len, char* buff, size_t buff_len);

extern const char _APP_KEY_HEADER[];

size_t _cb_read_buff(char *buffer, size_t size, size_t count, void *userdata);

#endif