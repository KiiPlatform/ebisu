#include "khc_impl.h"

#include <string.h>
#include <stdlib.h>

typedef enum header_parser_sts {
    parse_key,
    skip_separator,
    extract_value
} header_parser_sts;

int _contains_chunked(const char* str, size_t str_length) {
    const char upper_chunked[] = "CHUNKED";
    const char lower_chunked[] = "chunked";
    for (int i=0; i<str_length; ++i) {
        const char* start = str + i;
        for (int j=0; j<strlen(upper_chunked); ++j) {
            if (start[j] == upper_chunked[j] || start[j] == lower_chunked[j]) {
                if (j == strlen(upper_chunked)-1) {
                    return 1;
                }
                continue;
            } else {
                break;
            }
        }
    }
    return 0;
}

int _is_chunked_encoding(const char* header, size_t header_length) {
    const char upper_key[] = "TRANSFER-ENCODING";
    const char lower_key[] = "transfer-encoding";

    header_parser_sts sts = parse_key;
    for (int i=0; i<header_length; ++i) {
        switch(sts) {
            case parse_key:
                if (header[i] == ':') {
                    sts = skip_separator;
                    continue;
                }
                if (header[i] == upper_key[i] || header[i] == lower_key[i]) {
                    continue;
                }
                return 0;
            case skip_separator:
                if (header[i] == ' ' || header[i] == '\t') {
                    continue;
                } else {
                    sts = extract_value;
                }
                break;
            case extract_value:
                return _contains_chunked(&header[i-1], header_length - i + 1);
            default:
                return 0;
        }
    }
    return 0;
}

int _extract_content_length(const char* header, size_t header_length, size_t* out_content_length) {
    const char upper_key[] = "CONTENT-LENGTH";
    const char lower_key[] = "content-length";
    char* endptr = NULL;

    header_parser_sts sts = parse_key;
    for (int i=0; i<header_length; ++i) {
        switch(sts) {
            case parse_key:
                if (header[i] == ':') {
                    sts = skip_separator;
                    continue;
                }
                if (header[i] == upper_key[i] || header[i] == lower_key[i]) {
                    continue;
                }
                return 0;
            case skip_separator:
                if (header[i] == ' ' || header[i] == '\t') {
                    continue;
                } else {
                    sts = extract_value;
                }
                break;
            case extract_value:
                endptr = (char*)&header[i-1];
                *out_content_length = strtol(&header[i-1], &endptr, 10);
                return 1;
            default:
                return 0;
        }
    }
    return 0;
}
