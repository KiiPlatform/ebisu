#ifndef __large_test__
#define __large_test__

#include <chrono>
#include <functional>
#include "kii.h"
#include "secure_socket_impl.h"

namespace kiiltest {

const char DEFAULT_SITE[] = "api-jp.kii.com";
// APP Owner: satoshi.kumano@kii.com
const char APP_ID[] = "b6t9ai81zb3s";

inline void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        void* http_ssl_ctx,
        void* mqtt_ssl_ctx,
        jkii_resource_t* resource)
{
    kii_init(kii, DEFAULT_SITE, APP_ID);

    kii_set_buff(kii, buffer, buffer_size);

    kii_set_http_cb_sock_connect(kii, ebisu::ltest::ssl::cb_connect, http_ssl_ctx);
    kii_set_http_cb_sock_send(kii, ebisu::ltest::ssl::cb_send, http_ssl_ctx);
    kii_set_http_cb_sock_recv(kii, ebisu::ltest::ssl::cb_recv, http_ssl_ctx);
    kii_set_http_cb_sock_close(kii, ebisu::ltest::ssl::cb_close, http_ssl_ctx);

    kii_set_mqtt_cb_sock_connect(kii, ebisu::ltest::ssl::cb_connect, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_send(kii, ebisu::ltest::ssl::cb_send, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_recv(kii, ebisu::ltest::ssl::cb_recv, mqtt_ssl_ctx);
    kii_set_mqtt_cb_sock_close(kii, ebisu::ltest::ssl::cb_close, mqtt_ssl_ctx);
    kii_set_json_parser_resource(kii, resource);

    kii->_author.author_id[0] = '\0';
    kii->_author.access_token[0] = '\0';
}

inline long long current_time() {
    auto now = std::chrono::system_clock::now();
    return now.time_since_epoch().count() / 1000;
}

class RWFunc {
public:
    std::function<size_t(char *buffer, size_t size, size_t count, void *userdata)> on_read;
    std::function<size_t(char *buffer, size_t size, size_t count, void *userdata)> on_write;
};

inline size_t read_cb(char *buffer, size_t size, size_t count, void *userdata) {
    RWFunc* ctx = (RWFunc*)userdata;
    return ctx->on_read(buffer, size, count, userdata);
}

inline size_t write_cb(char *buffer, size_t size, size_t count, void *userdata) {
    RWFunc* ctx = (RWFunc*)userdata;
    return ctx->on_write(buffer, size, count, userdata);
}

} // namespace kiiltest

#endif
