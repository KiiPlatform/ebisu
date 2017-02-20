#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii_cl_parser.h"

/* Check whether the given string is content-length header.
 * @param header should point a first char of the header line.
 * @return
 * 1: given header is content-length header.
 * 0: given header is not content-length header.
 */
static int check(char* header) {
    const char* lower = "content-length";
    const char* upper = "CONTENT-LENGTH";
    int ret = 0;
    int i = 0;
    for (i = 0; i < strlen(lower); i++) {
        if (*header == lower[i] || *header == upper[i]) {
            header++;
            continue;
        }
        break;
    }
    if (i == strlen(lower)) {
        ret = 1;
    }
    return ret;
}

/** Get the value of content-length
 * @param header it should be a valid content-lenght header.
 * @return content-length value. 0 is returned when failed to parse number.
 */
static long value(char* header) {
    const char* lower = "content-length";
    char* next = header + strlen(lower);
    int state = 0;
    int idx = 0;
    char temp[16]; 
    temp[0] = '\0';

    while(idx < sizeof(temp)-1) {
        if (state == 0) {
            if (*next < '1' || '9' < *next) {
                next++;
            } else {
                state = 1;
                temp[idx] = *next;
                temp[idx+1] = '\0';
                idx++;
                next++;
            }
        } else if (state == 1) {
            if (*next < '0' || '9' < *next) {
                break;
            } else {
                temp[idx] = *next;
                temp[idx+1] = '\0';
                idx++;
                next++;
            }
        }
    }
    return atol(temp);
}

/** Skip to next header.
 * @param header buffer should be null terminated string.
 * @return next header pointer.
 * NULL if no next header exists.
 */
static char* skip(char* header) {
    while (*header != '\0') {
        if (*header == '\r') {
            char* next = header + 1;
            if (*next == '\n') {
                if (*(next+1) == '\r' && *(next+2) == '\n') {
                    // Reached to body part.
                    return NULL;
                } else {
                    return next + 1;
                }
            }
        }
        header++;
    }
    return NULL;
}

/**
 * @param HTTP response header part. Should be ended with \r\n\r\n
 * or null terminated.
 * @return content-length if present. Otherwise 0.
 */
long kii_parse_content_length(char* buffer) {
    char* next = buffer;
    int ret = 0;
    while(1) {
        next = skip(next);
        if (next == NULL) {
            break;
        }
        ret = check(next);
        if (ret == 1) {
            return value(next);
            break;
        }
    }
    // Content-Length header not found.
    return 0;
}

/* vim:set ts=4 sts=4 sw=4 et fenc=UTF-8 ff=unix: */
