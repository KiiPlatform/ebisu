#ifndef __large_test__
#define __large_test__

#include <algorithm>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <picojson.h>
#include "catch_amalgamated.hpp"
#include "kii.h"
#include "khc.h"
#include "secure_socket_impl.h"
#include "../test_env.h"

namespace kiiltest {

inline void init(
        kii_t* kii,
        char* buffer,
        int buffer_size,
        ebisu::ltest::ssl::SSLData* http_ssl_ctx,
        ebisu::ltest::ssl::SSLData* mqtt_ssl_ctx)
{
    kii_init(kii);
    kii_set_site(kii, DEFAULT_SITE);
    kii_set_app_id(kii, APP_ID);

    kii_set_buff(kii, buffer, buffer_size);

#ifdef PLAIN_HTTP
    kii_enable_insecure_http(kii, true);
#endif
    kii_set_http_socket(kii, http_ssl_ctx->http());
    kii_set_mqtt_socket(kii, mqtt_ssl_ctx->tls());
    kii_set_clock(kii, ebisu::ltest::test_clock());
}

inline kii_author_t author(kii_t* kii) {
    kii_author_t author;
    kii_get_author(kii, &author);
    return author;
}

// Microseconds since the epoch, which the tests name what they create by, so
// that the CI cells running at once against the same application never pick
// the same name.
inline long long current_time() {
    return ebisu::ltest::wall_us();
}

// The response a request gets that kii_t holds: an error, of the status, and
// in the body, an errorCode, which is one of those expected.
inline void check_error_response(
    kii_t& kii,
    char* buff,
    const int res,
    const int expected_status_code,
    const std::vector<std::string>& expected_error_codes
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
    const std::string code = errorCode.get<std::string>();
    INFO( "errorCode: " << code );
    REQUIRE(std::find(expected_error_codes.begin(), expected_error_codes.end(), code) !=
            expected_error_codes.end());
}

inline void check_error_response(
    kii_t& kii,
    char* buff,
    const int res,
    const int expected_status_code,
    const std::string& expected_error_code
    ) {
    check_error_response(kii, buff, res, expected_status_code,
            std::vector<std::string>{ expected_error_code });
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
