// An HTTP response, in reads of any length, through buffers of any size.
#include <khc.h>
#include <stdlib.h>
#include <vector>
#include "fuzz_input.h"

namespace {

struct Socket {
    ebisu_socket_t base;
    FuzzSocket fuzz;
};

ebisu_sock_code_t sock_connect(ebisu_socket_t*, const char*, uint16_t) {
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t sock_send(ebisu_socket_t*, const char*, size_t length, size_t* out_sent_length) {
    *out_sent_length = length;
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t sock_recv(ebisu_socket_t* self, char* buffer, size_t length, size_t* out_actual_length) {
    *out_actual_length = ((Socket*)self)->fuzz.read(buffer, length);
    return EBISU_SOCK_OK;
}

ebisu_sock_code_t sock_close(ebisu_socket_t*) {
    return EBISU_SOCK_OK;
}

const ebisu_socket_ops_t SOCKET_OPS = { sock_connect, sock_send, sock_recv, sock_close, NULL };

// A line khc gives is a status line or a header, which holds no line ending
// and no NUL: one that did would have been read another way by something else
// on the way.
size_t on_header(char* buffer, size_t size, void*) {
    for (size_t i = 0; i < size; ++i) {
        if (buffer[i] == '\r' || buffer[i] == '\n' || buffer[i] == '\0') {
            abort();
        }
    }
    return size;
}

size_t on_write(char*, size_t size, void*) {
    return size;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput input = { data, size };
    // Exactly these sizes, so that ASan sees one byte past them.
    std::vector<char> stream_buff(8 + input.take());
    std::vector<char> resp_header_buff(16 + input.take());
    size_t max_read = 1 + input.take();
    const char* method = (input.take() & 1) ? "HEAD" : "GET";

    khc http;
    khc_init(&http);
    khc_set_stream_buff(&http, stream_buff.data(), stream_buff.size());
    khc_set_resp_header_buff(&http, resp_header_buff.data(), resp_header_buff.size());
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, method);
    khc_set_path(&http, "/");
    khc_set_req_headers(&http, NULL);

    Socket socket = { { &SOCKET_OPS }, { input.data, input.size, max_read, 0 } };
    khc_set_socket(&http, &socket.base);
    khc_set_cb_header(&http, on_header, NULL);
    khc_set_cb_write(&http, on_write, NULL);

    khc_perform(&http);
    return 0;
}
