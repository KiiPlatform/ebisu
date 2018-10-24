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


#ifdef __cplusplus
}
#endif
#endif // __khc_impl__