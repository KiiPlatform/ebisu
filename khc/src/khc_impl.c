#include "khc_impl.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum header_parser_sts {
    parse_key,
    skip_separator
} header_parser_sts;

// Transfer-Encoding is a list of transfer codings, and the body is chunked when
// chunked is the last of them (RFC 9112, section 6.3). Empty list elements and
// whitespace around a coding do not count. khc cannot remove any other coding.
static int _read_transfer_encoding_value(const char* value, size_t value_length) {
    const char upper_chunked[] = "CHUNKED";
    const char lower_chunked[] = "chunked";
    const size_t chunked_length = sizeof(upper_chunked) - 1;

    size_t end = value_length;
    while (end > 0 && (value[end-1] == ' ' || value[end-1] == '\t' || value[end-1] == ',')) {
        --end;
    }
    size_t start = end;
    while (start > 0 && value[start-1] != ',') {
        --start;
    }
    while (start < end && (value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }
    if (end - start != chunked_length) {
        return -1;
    }
    for (size_t i = 0; i < chunked_length; ++i) {
        if (value[start+i] != upper_chunked[i] && value[start+i] != lower_chunked[i]) {
            return -1;
        }
    }
    return 1;
}

// Returns 1 for a Transfer-Encoding header whose final coding is chunked, 0 for
// any other header and -1 for a Transfer-Encoding header whose final coding is
// not chunked, which leaves the body in a coding khc cannot remove.
int _is_chunked_encoding(const char* header, size_t header_length) {
    const char upper_key[] = "TRANSFER-ENCODING";
    const char lower_key[] = "transfer-encoding";
    const size_t key_length = sizeof(upper_key) - 1;

    header_parser_sts sts = parse_key;
    for (int i=0; i<header_length; ++i) {
        switch(sts) {
            case parse_key:
                if (i == key_length) {
                    if (header[i] != ':') {
                        return 0;
                    }
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
                }
                return _read_transfer_encoding_value(&header[i], header_length - i);
            default:
                return 0;
        }
    }
    // Transfer-Encoding with an empty value.
    return (sts == skip_separator) ? -1 : 0;
}

// Content-Length is 1*DIGIT followed by optional whitespace. Read as a size_t
// whatever the width of long, which is 32 bits on the embedded targets.
static int _read_content_length_value(const char* value, size_t value_length, size_t* out_content_length) {
    size_t content_length = 0;
    size_t i = 0;
    for (; i < value_length && value[i] >= '0' && value[i] <= '9'; ++i) {
        size_t digit = value[i] - '0';
        if (content_length > (SIZE_MAX - digit) / 10) {
            return -1;
        }
        content_length = content_length * 10 + digit;
    }
    if (i == 0) {
        return -1;
    }
    for (; i < value_length; ++i) {
        if (value[i] != ' ' && value[i] != '\t') {
            return -1;
        }
    }
    *out_content_length = content_length;
    return 1;
}

// Returns 1 for a Content-Length header, 0 for any other header and -1 for a
// Content-Length header whose value is not a length that fits in a size_t.
int _extract_content_length(const char* header, size_t header_length, size_t* out_content_length) {
    const char upper_key[] = "CONTENT-LENGTH";
    const char lower_key[] = "content-length";
    const size_t key_length = sizeof(upper_key) - 1;

    header_parser_sts sts = parse_key;
    for (int i=0; i<header_length; ++i) {
        switch(sts) {
            case parse_key:
                if (i == key_length) {
                    if (header[i] != ':') {
                        return 0;
                    }
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
                }
                return _read_content_length_value(&header[i], header_length - i, out_content_length);
            default:
                return 0;
        }
    }
    // Content-Length with an empty value.
    return (sts == skip_separator) ? -1 : 0;
}

static int _hex_digit_value(char c) {
    if ('0' <= c && c <= '9') {
        return c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

int _read_chunk_size(const char* buff, size_t buff_size, size_t* out_chunk_size) {
    int crlf = 0;
    size_t line_size = 0;
    for(size_t i = 0; i < buff_size; ++i) {
        if (buff[i] == '\r') {
            crlf = 1;
            continue;
        } else if (buff[i] == '\n' && crlf == 1) {
            crlf = 2;
            line_size = i - 1;
            break;
        }
        crlf = 0;
    }
    if (crlf != 2) {
        return 0;
    }

    // chunk-size is 1*HEXDIG, and must fit in a size_t.
    size_t chunk_size = 0;
    size_t i = 0;
    for (; i < line_size; ++i) {
        int digit = _hex_digit_value(buff[i]);
        if (digit < 0) {
            break;
        }
        if (chunk_size > SIZE_MAX >> 4) {
            return -1;
        }
        chunk_size = (chunk_size << 4) | digit;
    }
    if (i == 0) {
        return -1;
    }
    // Optional whitespace, then a chunk extension, which is ignored.
    while (i < line_size && (buff[i] == ' ' || buff[i] == '\t')) {
        ++i;
    }
    if (i < line_size && buff[i] != ';') {
        return -1;
    }
    for (; i < line_size; ++i) {
        // HTTP allows no control character but HTAB in an extension, and the
        // caller finds the end of the line with strstr, which stops at a NUL.
        unsigned char c = buff[i];
        if ((c < 0x20 && c != '\t') || c == 0x7f) {
            return -1;
        }
    }
    *out_chunk_size = chunk_size;
    return 1;
}

int _is_header_present(const char* header, const char* buff, size_t buff_size) {
    int header_length = strlen(header);
    for (int i = 0; i < buff_size; ++i) {
        if (buff[i] == ':') {
            return i == header_length;
        }
        if (i < header_length) {
            char c = header[i];
            if (buff[i] != tolower(c) && buff[i] != toupper(c)) {
                break;
            }
        } else {
            break;
        }
    }
    return 0;
}
