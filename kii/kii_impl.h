#ifndef __kii_impl__
#define __kii_impl__

#include "kii.h"
#include "khc.h"

kii_code_t _convert_code(khc_code khc_c);

void _reset_buff(kii_t* kii);

/* Reads the thing's ID and access token, named id_name and token_name, from a
 * login's response into kii's author, only if the response has them both and
 * they fit: otherwise KII_ERR_PARSE_JSON, and the author is as it was. */
kii_code_t _kii_read_author(
        kii_t* kii,
        char* buff,
        size_t buff_size,
        const char* id_name,
        const char* token_name);

/* khc_perform() for kii's requests: every one is sent through it. */
khc_code _kii_perform(kii_t* kii);

void _req_headers_free_all(kii_t* kii);

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
