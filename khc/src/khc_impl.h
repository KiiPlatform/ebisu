#ifndef __khc_impl__
#define __khc_impl__
#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

int _contains_chunked(const char* str, size_t str_length);

int _is_chunked_encoding(const char* header, size_t header_length);

int _extract_content_length(const char* header, size_t header_length, size_t* out_content_length);

int _read_chunk_size(const char* buff, size_t buff_size, size_t* out_chunk_size);

int _is_header_present(const char* header, const char* buff, size_t buff_size);

#ifdef __cplusplus
}
#endif
#endif // __khc_impl__
