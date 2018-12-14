#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include <khc.h>
#include "secure_socket_impl.h"
#include "khc_state_impl.h"
#include "test_callbacks.h"

TEST_CASE( "HTTP Get" ) {
    khc http;
    khc_init(&http);
    khc_set_host(&http, "api-jp.kii.com");
    khc_set_method(&http, "GET");
    khc_set_method(&http, "");
    khc_set_req_headers(&http, NULL);

    ebisu::ltest::ssl::SSLData s_ctx;
    khc_set_cb_sock_connect(&http, ebisu::ltest::ssl::cb_connect, &s_ctx);
    khc_set_cb_sock_send(&http, ebisu::ltest::ssl::cb_send, &s_ctx);
    khc_set_cb_sock_recv(&http, ebisu::ltest::ssl::cb_recv, &s_ctx);
    khc_set_cb_sock_close(&http, ebisu::ltest::ssl::cb_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_read_called = 0;
    io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        // No req body.
        return 0;
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
    io_ctx.on_write = [=, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        REQUIRE ( size == 2 );
        REQUIRE ( strncmp(buffer, "{}", 2) == 0 );
        return size;
    };

    khc_code res = khc_perform(&http);
    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 404 );
    REQUIRE( on_read_called == 1 );
    REQUIRE( on_header_called > 1 );
    REQUIRE( on_write_called == 1 );
}
