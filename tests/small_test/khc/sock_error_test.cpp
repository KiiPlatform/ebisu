#include "catch.hpp"
#include <khc.h>
#include "http_script.h"
#include <string>

// A socket failure was KHC_ERR_SOCK_CONNECT, _SEND or _RECV and nothing more:
// a host that didn't resolve and a certificate refused looked alike. khc now
// asks the socket's last_error why, right after the failure, before closing.

namespace {

struct Fixture {
    khc http;
    khct::HttpScript script;
    khct::cb::IOCtx io;
    int closes = 0;
    // What the socket says, and whether it was asked before the close.
    ebisu_sock_error_t says = EBISU_SOCK_ERROR_NONE;
    long says_detail = 0;
    int asked = 0;
    int asked_after_close = 0;

    Fixture() {
        khc_init(&http);
        khc_set_host(&http, "api.kii.com");
        khc_set_method(&http, "GET");
        khc_set_path(&http, "/api/apps");
        khc_set_req_headers(&http, NULL);
        script.attach(&http);
        io.on_header = [](char*, size_t size, void*) { return size; };
        io.on_write = [](char*, size_t size, void*) { return size; };
        khc_set_cb_write(&http, khct::cb::cb_write, &io);
        khc_set_cb_header(&http, khct::cb::cb_header, &io);
        script.ctx.on_close = [this](void*) {
            ++closes;
            return EBISU_SOCK_OK;
        };
        script.ctx.on_last_error = [this](long* out_detail) {
            ++asked;
            if (closes > 0) {
                ++asked_after_close;
            }
            *out_detail = says_detail;
            return says;
        };
    }
};

}

TEST_CASE( "khc says why the socket failed" ) {
    Fixture f;
    long detail = -1;

    SECTION("connecting, as the socket says") {
        f.says = EBISU_SOCK_ERROR_DNS;
        f.says_detail = -2;
        // No response for the connection, which refuses it.
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_CONNECT );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_DNS );
        CHECK( detail == -2 );
        CHECK( f.asked == 1 );
    }

    SECTION("sending, asked before the close") {
        f.script.responses = { khct::http_response(200, "{}") };
        f.script.ctx.on_send = [](void*, const char*, size_t, size_t*) { return EBISU_SOCK_FAIL; };
        f.says = EBISU_SOCK_ERROR_CLOSED;
        f.says_detail = 32;
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_SEND );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_CLOSED );
        CHECK( detail == 32 );
        CHECK( f.closes == 1 );
        CHECK( f.asked == 1 );
        CHECK( f.asked_after_close == 0 );
    }

    SECTION("receiving") {
        f.script.responses = { khct::http_response(200, "{}") };
        f.script.ctx.on_recv = [](void*, char*, size_t, size_t* out_actual_length) {
            *out_actual_length = 0;
            return EBISU_SOCK_FAIL;
        };
        f.says = EBISU_SOCK_ERROR_TIMEOUT;
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_RECV );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_TIMEOUT );
        CHECK( detail == 0 );
    }

    SECTION("a connection closed before the response ended, from khc") {
        f.script.responses = { "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n{}" };
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_RECV );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_CLOSED );
        CHECK( detail == 0 );
        CHECK( f.asked == 0 );
    }

    SECTION("a socket that can't tell") {
        f.script.ctx.ops.last_error = NULL;
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_CONNECT );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( detail == 0 );
    }

    SECTION("nothing, after a request that went well, and none asked") {
        f.says = EBISU_SOCK_ERROR_DNS;
        CHECK( khc_perform(&f.http) == KHC_ERR_SOCK_CONNECT );
        f.script.responses = { "", khct::http_response(200, "{}") };
        khc_reset_except_request(&f.http);
        CHECK( khc_get_sock_error(&f.http, NULL) == EBISU_SOCK_ERROR_NONE );
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( khc_get_sock_error(&f.http, &detail) == EBISU_SOCK_ERROR_NONE );
        CHECK( f.asked == 1 );
    }
}
