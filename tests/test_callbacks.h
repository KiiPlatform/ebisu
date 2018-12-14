#include <khc_socket_callback.h>
#include <functional>

namespace khct {
namespace cb {

struct SockCtx {
    std::function<khc_sock_code_t(void* socket_context, const char* host, unsigned int port)> on_connect;
    std::function<khc_sock_code_t(void* socket_context, const char* buffer, size_t length, size_t* out_sent_length)> on_send;
    std::function<khc_sock_code_t(void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length)> on_recv;
    std::function<khc_sock_code_t(void* socket_context)> on_close;
};

struct IOCtx {
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_read;
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_header;
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_write;
};

inline khc_sock_code_t mock_connect(void* socket_context, const char* host, unsigned int port) {
    SockCtx* ctx = (SockCtx*)socket_context;
    return ctx->on_connect(socket_context, host, port);
}

inline khc_sock_code_t mock_send (void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
    SockCtx* ctx = (SockCtx*)socket_context;
    return ctx->on_send(socket_context, buffer, length, out_sent_length);
}

inline khc_sock_code_t mock_recv(void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
    SockCtx* ctx = (SockCtx*)socket_context;
    return ctx->on_recv(socket_context, buffer, length_to_read, out_actual_length);
}

inline khc_sock_code_t mock_close(void* socket_context) {
    SockCtx* ctx = (SockCtx*)socket_context;
    return ctx->on_close(socket_context);
}

inline size_t cb_write(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_write(buffer, size, userdata);
}

inline size_t cb_read(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_read(buffer, size, userdata);
}

inline size_t cb_header(char *buffer, size_t size, void *userdata) {
    IOCtx* ctx = (IOCtx*)(userdata);
    return ctx->on_header(buffer, size, userdata);
}

inline void cb_delay_ms(unsigned int msec, void *userdata) {
}

struct PushCtx {
    std::function<void(const char *message, size_t message_length)> on_push;
};

inline void cb_push(const char* message, size_t message_length, void *userdata) {
    PushCtx* ctx = (PushCtx*)(userdata);
    ctx->on_push(message, message_length);
}
}
}
