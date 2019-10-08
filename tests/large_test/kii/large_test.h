#ifndef __large_test__
#define __large_test__

#include <chrono>
#include <functional>
#include "kii.h"
#include "secure_socket_impl.h"

namespace kiiltest {

// stringify the macro value
// ref. https://gcc.gnu.org/onlinedocs/gcc-4.8.5/cpp/Stringification.html
#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef _DEFAULT_SITE
const char DEFAULT_SITE[] = "api-jp.kii.com";
#else
const char DEFAULT_SITE[] = XSTR(_DEFAULT_SITE);
#endif
#ifndef _APP_ID
// APP Owner: satoshi.kumano@kii.com
const char APP_ID[] = "b6t9ai81zb3s";
#else
const char APP_ID[] = XSTR(_APP_ID);
#endif

inline void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        void* http_ssl_ctx,
        void* mqtt_ssl_ctx,
        jkii_resource_t* resource)
{
    kii_init(kii);
    kii_set_site(kii, DEFAULT_SITE);
    kii_set_app_id(kii, APP_ID);

    kii_set_buff(kii, buffer, buffer_size);

#ifdef PLAIN_HTTP
    kii_enable_insecure_http(kii, KII_TRUE);
    kii_set_cb_http_sock_connect(kii, ebisu::ltest::tcp::cb_connect, http_ssl_ctx);
    kii_set_cb_http_sock_send(kii, ebisu::ltest::tcp::cb_send, http_ssl_ctx);
    kii_set_cb_http_sock_recv(kii, ebisu::ltest::tcp::cb_recv, http_ssl_ctx);
    kii_set_cb_http_sock_close(kii, ebisu::ltest::tcp::cb_close, http_ssl_ctx);
#else
    kii_set_cb_http_sock_connect(kii, ebisu::ltest::ssl::cb_connect, http_ssl_ctx);
    kii_set_cb_http_sock_send(kii, ebisu::ltest::ssl::cb_send, http_ssl_ctx);
    kii_set_cb_http_sock_recv(kii, ebisu::ltest::ssl::cb_recv, http_ssl_ctx);
    kii_set_cb_http_sock_close(kii, ebisu::ltest::ssl::cb_close, http_ssl_ctx);
#endif

    kii_set_cb_mqtt_sock_connect(kii, ebisu::ltest::ssl::cb_connect, mqtt_ssl_ctx);
    kii_set_cb_mqtt_sock_send(kii, ebisu::ltest::ssl::cb_send, mqtt_ssl_ctx);
    kii_set_cb_mqtt_sock_recv(kii, ebisu::ltest::ssl::cb_recv, mqtt_ssl_ctx);
    kii_set_cb_mqtt_sock_close(kii, ebisu::ltest::ssl::cb_close, mqtt_ssl_ctx);
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
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_read;
    std::function<size_t(char *buffer, size_t size, void *userdata)> on_write;
};

inline size_t read_cb(char *buffer, size_t size, void *userdata) {
    RWFunc* ctx = (RWFunc*)userdata;
    return ctx->on_read(buffer, size, userdata);
}

inline size_t write_cb(char *buffer, size_t size, void *userdata) {
    RWFunc* ctx = (RWFunc*)userdata;
    return ctx->on_write(buffer, size, userdata);
}

} // namespace kiiltest

#endif
