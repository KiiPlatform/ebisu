#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "kii_cl_parser.h"

static const char* lower = "content-length";
static const char* upper = "CONTENT-LENGTH";
static const size_t cl_length = 14;

/* Check whether the given string is content-length header.
 * @param header should point a first char of the header line.
 * @return
 * 1: given header is content-length header.
 * 0: given header is not content-length header.
 */
static int check(const char* header) {
    int ret = 0;
    int i = 0;
    for (i = 0; i < cl_length; i++) {
        if (*header == lower[i] || *header == upper[i]) {
            header++;
            continue;
        }
        break;
    }
    if (i == cl_length) {
        ret = 1;
    }
    return ret;
}

/** Get the value of content-length
 * @param header it should be a valid content-lenght header.
 * @return content-length value. 0 is returned when failed to parse number.
 */
static unsigned long value(const char* header) {
    char* start = (char*)header + cl_length;
    char* next = start;
    const unsigned int MAX_SEARCH_NUM = 10;
    const unsigned int MAX_FIGURES = 10;
    int i = 1;
    long ret = 0;

    // Search for number.
    while (*next < '1' || '9' < *next) {
        ++next;
        // To avoid infinite loop if invalid header is given.
        if (next - start > MAX_SEARCH_NUM) {
            return 0;
        }
    }
    ret = *next - '0';
    for (i=1; i < MAX_FIGURES; ++i, ++next) {
        char* nn = next + 1;
        if ('0' <= *nn &&  *nn <= '9') {
            ret = ret * 10 + (*nn - '0');
        } else {
            break;
        }
    }
    return ret;
}

/** Skip to next header.
 * @param header buffer should be null terminated string.
 * @return next header pointer.
 * NULL if no next header exists.
 */
static char* skip(const char* header) {
    while (*header != '\0') {
        if (*header == '\r') {
            char* next = (char*)header + 1;
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
unsigned long kii_parse_content_length(const char* buffer) {
    char* next = (char*)buffer;
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
