#include "khc_impl.h"

#include <ebisu_str.h>
#include <stdint.h>
#include <string.h>

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
    // Optional whitespace before a chunk extension, which is ignored, and
    // before nothing else (RFC 9112, section 7.1.1).
    size_t digits_end = i;
    while (i < line_size && (buff[i] == ' ' || buff[i] == '\t')) {
        ++i;
    }
    if (i < line_size ? buff[i] != ';' : i != digits_end) {
        return -1;
    }
    for (; i < line_size; ++i) {
        // HTTP allows no control character but HTAB in an extension.
        unsigned char c = buff[i];
        if ((c < 0x20 && c != '\t') || c == 0x7f) {
            return -1;
        }
    }
    *out_chunk_size = chunk_size;
    return 1;
}

static bool _is_field_char(unsigned char c) {
    return c == '\t' || (c >= 0x20 && c != 0x7f);
}

bool _is_field_text(const char* text, size_t text_length) {
    for (size_t i = 0; i < text_length; ++i) {
        if (!_is_field_char((unsigned char)text[i])) {
            return false;
        }
    }
    return true;
}

/* tchar of RFC 9110, section 5.6.2. */
static bool _is_token_char(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        return true;
    }
    return c != '\0' && strchr("!#$%&'*+-.^_`|~", c) != NULL;
}

/* The length of the name a header starts with, up to its colon, or 0 if it
 * doesn't start with a name and a colon within data_length. */
static size_t _header_name_length(const char* data, size_t data_length) {
    size_t i = 0;
    while (i < data_length && _is_token_char((unsigned char)data[i])) {
        ++i;
    }
    return (i > 0 && i < data_length && data[i] == ':') ? i : 0;
}

bool _is_header_line(const char* line, size_t line_length) {
    size_t name_length = _header_name_length(line, line_length);
    return name_length > 0 &&
        _is_field_text(&line[name_length + 1], line_length - name_length - 1);
}

bool _is_header_start(const char* data, size_t data_length) {
    if (data_length > 0 && data[data_length - 1] == '\r') {
        --data_length;
    }
    return _is_header_line(data, data_length);
}

bool _is_status_line(const char* line, size_t line_length) {
    /* "HTTP/1.1 200", the least a status line is. */
    const size_t least = 12;
    if (line_length < least || memcmp(line, "HTTP/1.", 7) != 0 ||
            line[7] < '0' || line[7] > '9' || line[8] != ' ') {
        return false;
    }
    for (size_t i = 9; i < 12; ++i) {
        if (line[i] < '0' || line[i] > '9') {
            return false;
        }
    }
    if (line_length == least) {
        return true;
    }
    return line[least] == ' ' && _is_field_text(&line[least + 1], line_length - least - 1);
}

/* The name, before the colon, compared but for the case of ASCII letters: not
   with tolower() and toupper(), which follow the locale, in which I and i
   needn't be each other's case. In a Turkish one, glibc's toupper('i') is
   'i'. */
int _is_header_present(const char* header, const char* buff, size_t buff_size) {
    const char* colon = memchr(buff, ':', buff_size);
    ebisu_span_t name;
    if (colon == NULL) {
        return 0;
    }
    name.ptr = buff;
    name.len = (size_t)(colon - buff);
    return ebisu_span_eq_nocase(name, header) ? 1 : 0;
}
