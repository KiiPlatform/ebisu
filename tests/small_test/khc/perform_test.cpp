#include "catch.hpp"
#include <khc.h>
#include "http_script.h"

#include <string>
#include <string.h>
#include <vector>

// Without khc_reset_except_cb() in between, khc_perform() read the next
// response with what the last one had left behind: a Content-Length body's
// count of the bytes received, or a close-delimited body's end of stream, and
// ended the next body early, returning KHC_ERR_OK. It now starts afresh.
TEST_CASE( "khc_perform starts afresh after a response, without a reset" ) {
    const std::string first_body(2000, 'a');
    const std::string next_body(1500, 'b');
    const std::string first[] = {
        khct::http_response(200, first_body),
        "HTTP/1.1 200 OK\r\n\r\n" + first_body,
    };
    const std::string next[] = {
        khct::http_response(201, next_body),
        "HTTP/1.1 201 Created\r\n\r\n" + next_body,
    };
    const int i = GENERATE(0, 1);
    const int j = GENERATE(0, 1);
    CAPTURE( i, j );

    khc http;
    khc_init(&http);
    khct::HttpScript script;
    script.attach(&http);
    std::string received;
    khct::cb::IOCtx io;
    io.on_write = [&](char* buffer, size_t size, void*) {
        received.append(buffer, size);
        return size;
    };
    io.on_header = [](char*, size_t size, void*) { return size; };
    khc_set_cb_write(&http, khct::cb::cb_write, &io);
    khc_set_cb_header(&http, khct::cb::cb_header, &io);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/x");
    // The third connection is refused.
    script.responses = { first[i], next[j], "" };

    REQUIRE( khc_perform(&http) == KHC_ERR_OK );
    REQUIRE( received == first_body );
    received.clear();

    CHECK( khc_perform(&http) == KHC_ERR_OK );
    CHECK( khc_get_status_code(&http) == 201 );
    CHECK( received.size() == next_body.size() );
    CHECK( received == next_body );
    REQUIRE( script.requests.size() == 2 );
    CHECK( script.requests[0] == script.requests[1] );

    // A request that fails before reaching the server has no status code of
    // the one before.
    CHECK( khc_perform(&http) == KHC_ERR_SOCK_CONNECT );
    CHECK( khc_get_status_code(&http) == 0 );
}

// Reads into a response header buffer of size 0 weren't bounded by it, the
// room left in it wrapping around, and a stream buffer of size 0 read nothing,
// which ends a body. Either now fails the request before it connects. A NULL
// buffer is still allocated, whatever size comes with it.
TEST_CASE( "khc_perform needs room in the buffers it is given" ) {
    std::vector<char> buffer(16);
    const int given = GENERATE(0, 1, 2, 3, 4);
    CAPTURE( given );

    khc http;
    khc_init(&http);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    int on_connect_called = 0;
    khct::cb::SockCtx s_ctx;
    s_ctx.on_connect = [&on_connect_called](void*, const char*, unsigned int) {
        ++on_connect_called;
        return KHC_SOCK_FAIL;
    };
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);
    khct::cb::IOCtx io_ctx;
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    bool connects = true;
    switch (given) {
        case 0:
            khc_set_resp_header_buff(&http, buffer.data(), 0);
            connects = false;
            break;
        case 1:
            khc_set_stream_buff(&http, buffer.data(), 0);
            connects = false;
            break;
        case 2:
            khc_set_resp_header_buff(&http, buffer.data(), 1);
            break;
        case 3:
            khc_set_stream_buff(&http, buffer.data(), 1);
            break;
        case 4:
            khc_set_resp_header_buff(&http, NULL, 0);
            khc_set_stream_buff(&http, NULL, 0);
            break;
    }
    CHECK( khc_perform(&http) == (connects ? KHC_ERR_SOCK_CONNECT : KHC_ERR_FAIL) );
    CHECK( on_connect_called == (connects ? 1 : 0) );
}
