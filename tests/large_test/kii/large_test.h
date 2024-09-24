#ifndef __large_test__
#define __large_test__

#include <string>
#include <chrono>
#include <functional>
#include <picojson.h>
#include <catch.hpp>
#include "kii.h"
#include "khc.h"
#include "secure_socket_impl.h"
#include "../test_env.h"

namespace kiiltest {

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

inline void check_error_response(
    kii_t& kii,
    char* buff,
    const int res, 
    const int expected_status_code, 
    const std::string& expected_error_code
    ) {
    REQUIRE(res == KII_ERR_RESP_STATUS);
    
    int actual_status_code = khc_get_status_code(&kii._khc);
    REQUIRE(actual_status_code == expected_status_code);
    
    picojson::value v;
    auto err_str = picojson::parse(v, buff);
    REQUIRE(err_str.empty());
    REQUIRE(v.is<picojson::object>());
    
    picojson::object obj = v.get<picojson::object>();
    auto errorCode = obj.at("errorCode");
    REQUIRE(errorCode.is<std::string>());
    REQUIRE(errorCode.get<std::string>() == expected_error_code);
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
