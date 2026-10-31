#ifndef __fuzz_input__
#define __fuzz_input__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// The first bytes of a fuzzer's input choose how a run is set up, and what is
// left is what it reads.
struct FuzzInput {
    const uint8_t* data;
    size_t size;

    uint8_t take() {
        if (size == 0) {
            return 0;
        }
        uint8_t byte = *data;
        ++data;
        --size;
        return byte;
    }
};

// Reads what a connection brings, in reads whose length the input chooses,
// until it closes. Nothing should go on reading from a closed connection: one
// that does has no other way to stop, so that is a finding, not a slow run.
// A few reads are fine, one for each interim response still in the buffers.
struct FuzzSocket {
    const uint8_t* data;
    size_t size;
    size_t max_read;
    int reads_after_close;

    static const int MAX_READS_AFTER_CLOSE = 64;

    size_t read(char* buffer, size_t length) {
        if (size == 0) {
            if (++reads_after_close > MAX_READS_AFTER_CLOSE) {
                abort();
            }
            return 0;
        }
        size_t n = length < max_read ? length : max_read;
        if (n > size) {
            n = size;
        }
        memcpy(buffer, data, n);
        data += n;
        size -= n;
        return n;
    }
};

#endif
