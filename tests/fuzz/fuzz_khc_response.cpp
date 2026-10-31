// An HTTP response, in reads of any length, through buffers of any size.
#include <khc.h>
#include <vector>
#include "fuzz_input.h"

namespace {

khc_sock_code_t sock_connect(void*, const char*, uint16_t) {
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_send(void*, const char*, size_t length, size_t* out_sent_length) {
    *out_sent_length = length;
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_recv(void* context, char* buffer, size_t length, size_t* out_actual_length) {
    *out_actual_length = ((FuzzSocket*)context)->read(buffer, length);
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_close(void*) {
    return KHC_SOCK_OK;
}

size_t on_header(char*, size_t size, void*) {
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

    FuzzSocket socket = { input.data, input.size, max_read, 0 };
    khc_set_cb_sock_connect(&http, sock_connect, NULL);
    khc_set_cb_sock_send(&http, sock_send, NULL);
    khc_set_cb_sock_recv(&http, sock_recv, &socket);
    khc_set_cb_sock_close(&http, sock_close, NULL);
    khc_set_cb_header(&http, on_header, NULL);
    khc_set_cb_write(&http, on_write, NULL);

    khc_perform(&http);
    return 0;
}
