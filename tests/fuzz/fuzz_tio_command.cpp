// A command as the handler receives it, with the actions it names run and their
// results sent.
#include <tio.h>
#include <command_parser.h>
#include <vector>
#include "fuzz_input.h"

namespace {

const char RESPONSE[] = "HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n";

struct Socket {
    size_t position;
};

khc_sock_code_t sock_connect(void* context, const char*, uint16_t) {
    ((Socket*)context)->position = 0;
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_send(void*, const char*, size_t length, size_t* out_sent_length) {
    *out_sent_length = length;
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_recv(void* context, char* buffer, size_t length, size_t* out_actual_length) {
    Socket* socket = (Socket*)context;
    size_t n = sizeof(RESPONSE) - 1 - socket->position;
    if (n > length) {
        n = length;
    }
    memcpy(buffer, RESPONSE + socket->position, n);
    socket->position += n;
    *out_actual_length = n;
    return KHC_SOCK_OK;
}

khc_sock_code_t sock_close(void*) {
    return KHC_SOCK_OK;
}

// Reads all it is given, for ASan to see the lengths are right, and fails every
// other action, for the path that reports a failure.
tio_bool_t on_action(tio_action_t* action, tio_action_err_t* err, tio_action_result_data_t* data, void* userdata) {
    size_t* sum = (size_t*)userdata;
    for (size_t i = 0; i < action->alias_length; ++i) {
        *sum += (unsigned char)action->alias[i];
    }
    for (size_t i = 0; i < action->action_name_length; ++i) {
        *sum += (unsigned char)action->action_name[i];
    }
    if (*sum & 1) {
        strcpy(err->err_message, "failed \"on purpose\"");
        return KII_FALSE;
    }
    return KII_TRUE;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzInput input = { data, size };
    // Exactly this size, so that ASan sees one byte past it.
    std::vector<char> http_buff(128 + input.take() * 8);
    std::vector<char> resp_header_buff(256);
    if (input.size == 0) {
        return 0;
    }
    const std::vector<char> command(input.data, input.data + input.size);

    tio_handler_t handler;
    tio_handler_init(&handler);
    jkii_token_t tokens[64];
    jkii_resource_t resource = { tokens, 64 };
    kii_set_buff(&handler._kii, http_buff.data(), http_buff.size());
    kii_set_json_parser_resource(&handler._kii, &resource);
    kii_set_app_id(&handler._kii, "app");
    kii_set_site(&handler._kii, "api.kii.com");
    khc_set_resp_header_buff(&handler._kii._khc, resp_header_buff.data(), resp_header_buff.size());
    strcpy(handler._kii._author.author_id, "th.1");

    Socket socket = { 0 };
    kii_set_cb_http_sock_connect(&handler._kii, sock_connect, &socket);
    kii_set_cb_http_sock_send(&handler._kii, sock_send, &socket);
    kii_set_cb_http_sock_recv(&handler._kii, sock_recv, &socket);
    kii_set_cb_http_sock_close(&handler._kii, sock_close, &socket);

    size_t sum = 0;
    _handle_command(&handler, command.data(), command.size(), on_action, &sum);
    return 0;
}
