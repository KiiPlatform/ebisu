#include "catch.hpp"
#include <khc.h>
#include "secure_socket_impl.h"
#include "khc_state_impl.h"
#include "test_callbacks.h"
#include "picojson.h"
#include <sstream>

TEST_CASE( "HTTP Post with error" ) {
    khc http;
    khc_init(&http);

    std::string app_id = "random_app";
    std::string email = "random_email@domain.local";
    std::string path = "/api/apps";
    std::string host = "api-jp.kii.com";
    std::string content_type = "Content-Type: application/vnd.kii.AppCreationRequest+json";

    khc_set_host(&http, host.c_str());
    khc_set_method(&http, "POST");
    khc_set_path(&http, path.c_str());

    // Prepare Req Body.
    picojson::object o;
    o.insert(std::make_pair("appID", app_id));
    o.insert(std::make_pair("emailAddress", email));
    o.insert(std::make_pair("description", "random"));
    picojson::value v(o);
    std::string req_body = v.serialize();
    size_t body_len = req_body.length();

    // Prepare Req Headers.
    khc_slist* headers = NULL;
    headers = khc_slist_append(headers, content_type.c_str(), content_type.length());

    khc_set_req_headers(&http, headers);

    ebisu::ltest::ssl::SSLData s_ctx;
#ifdef PLAIN_HTTP
    khc_enable_insecure(&http, 1);
    khc_set_cb_sock_connect(&http, ebisu::ltest::tcp::cb_connect, &s_ctx);
    khc_set_cb_sock_send(&http, ebisu::ltest::tcp::cb_send, &s_ctx);
    khc_set_cb_sock_recv(&http, ebisu::ltest::tcp::cb_recv, &s_ctx);
    khc_set_cb_sock_close(&http, ebisu::ltest::tcp::cb_close, &s_ctx);
#else
    khc_set_cb_sock_connect(&http, ebisu::ltest::ssl::cb_connect, &s_ctx);
    khc_set_cb_sock_send(&http, ebisu::ltest::ssl::cb_send, &s_ctx);
    khc_set_cb_sock_recv(&http, ebisu::ltest::ssl::cb_recv, &s_ctx);
    khc_set_cb_sock_close(&http, ebisu::ltest::ssl::cb_close, &s_ctx);
#endif

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_read_called = 0;
    std::istringstream iss(req_body);
    io_ctx.on_read = [=, &on_read_called, &iss](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        return iss.read(buffer, size).gcount();
    };

    int on_header_called = 0;
    io_ctx.on_header = [=, &on_header_called](char *buffer, size_t size, void *userdata) {
        ++on_header_called;
        // Ignore resp headers.
        char str[size + 1];
        strncpy(str, buffer, size);
        str[size] = '\0';
        printf("%s\n", str);
        return size;
    };

    int on_write_called = 0;
    std::ostringstream oss;
    io_ctx.on_write = [=, &on_write_called, &oss](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
        return size;
    };

    khc_code res = khc_perform(&http);
    khc_slist_free_all(headers);
    REQUIRE( khc_get_status_code(&http) == 401 );

    // Parse response body.
    auto resp_body = oss.str();
    auto err_str = picojson::parse(v, resp_body);
    REQUIRE ( err_str.empty() );
    REQUIRE ( v.is<picojson::object>() );
    picojson::object obj = v.get<picojson::object>();
    auto error_code = obj.at("errorCode");
    REQUIRE ( error_code.is<std::string>() );
    REQUIRE ( !error_code.get<std::string>().empty() );

    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( on_read_called == 2 );
    REQUIRE( on_header_called > 1 );
    REQUIRE( on_write_called == 2 );
}
