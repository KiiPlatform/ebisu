#include "catch.hpp"
#include <khc.h>
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <fstream>
#include <stdint.h>
#include <string.h>
#include <string>

TEST_CASE( "HTTP response test" ) {
    khc http;
    khc_init(&http);

    ifstream ifs;
    // Binary because the fixtures are CRLF, as HTTP is. In text mode Windows
    // hands them back with LF line endings, and Resp, which splits headers on
    // CRLF, takes the whole file for a single header.
    ifs.open("./data/resp-login.txt", ios::binary);

    khct::http::Resp resp(ifs);

    ifs.close();

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    // no set cb_read.
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, uint16_t port) {
        ++on_connect_called;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int on_send_called = 0;
    s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++on_send_called;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int on_read_called = 0;
    io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        FAIL();
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
        if (on_recv_called == 1)
            REQUIRE( length_to_read == 255 );
        if (on_recv_called == 2)
            REQUIRE( length_to_read == 236 );
        if (on_recv_called == 3)
            REQUIRE( length_to_read == 1024 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int on_header_called = 0;
    io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, void *userdata) {
        const char* header = resp.headers[on_header_called].c_str();
        size_t len = strlen(header);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, header, len) == 0 );
        ++on_header_called;
        return size;
    };

    istringstream iss = istringstream(resp.body);
    int on_write_called = 0;
    io_ctx.on_write = [=, &iss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        char body_buffer[size];
        memset(body_buffer, '\0', size);
        iss.read((char*)body_buffer, size);
        REQUIRE ( strncmp(buffer, body_buffer, size) == 0 );
        return size;
    };

    int on_close_called = 0;
    s_ctx.on_close = [=, &on_close_called](void* socket_ctx) {
        ++on_close_called;
        return KHC_SOCK_OK;
    };

    khc_code res = khc_perform(&http);
    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( on_connect_called == 1 );
    REQUIRE( on_send_called == 3 );
    REQUIRE( on_read_called == 0 );
    REQUIRE( on_recv_called == 4 );
    REQUIRE( on_header_called == 12 );
    REQUIRE( on_write_called > 0 );
    REQUIRE( on_close_called == 1 );
}

TEST_CASE( "Ignore HTTP status 100" ) {
    khc http;
    khc_init(&http);

    ifstream ifs;
    ifs.open("./data/resp-login.txt", ios::binary);

    khct::http::Resp resp(ifs, true);

    ifs.close();

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    // no set cb_read.
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, uint16_t port) {
        ++on_connect_called;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int on_send_called = 0;
    s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++on_send_called;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int on_read_called = 0;
    io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        FAIL();
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
        REQUIRE( length_to_read > 0 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int on_header_called = 0;
    io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, void *userdata) {
        const char* header = resp.headers[on_header_called].c_str();
        size_t len = strlen(header);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, header, len) == 0 );
        ++on_header_called;
        return size;
    };

    istringstream iss = istringstream(resp.body);
    int on_write_called = 0;
    io_ctx.on_write = [=, &iss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        char body_buffer[size];
        memset(body_buffer, '\0', size);
        iss.read((char*)body_buffer, size);
        REQUIRE ( strncmp(buffer, body_buffer, size) == 0 );
        return size;
    };

    int on_close_called = 0;
    s_ctx.on_close = [=, &on_close_called](void* socket_ctx) {
        ++on_close_called;
        return KHC_SOCK_OK;
    };

    khc_code res = khc_perform(&http);
    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( on_connect_called == 1 );
    REQUIRE( on_send_called == 3 );
    REQUIRE( on_read_called == 0 );
    REQUIRE( on_recv_called == 5 );
    REQUIRE( on_header_called == 14 );
    REQUIRE( on_write_called == 2 );
    REQUIRE( on_close_called == 1 );
}

TEST_CASE( "Small resp header skip test" ) {
    khc http;
    khc_init(&http);
    const size_t resp_header_buff_size = 120;
    char resp_header_buff[resp_header_buff_size];

    ifstream ifs;
    ifs.open("./data/resp-login.txt", ios::binary);

    khct::http::Resp resp(ifs);

    ifs.close();

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_resp_header_buff(&http, resp_header_buff, resp_header_buff_size);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    // no set cb_read.
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, uint16_t port) {
        ++on_connect_called;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int on_send_called = 0;
    s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++on_send_called;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int on_read_called = 0;
    io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        FAIL();
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
        if (on_recv_called == 1)
            REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int on_header_called = 0;
    io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, void *userdata) {
        int skip = 0;
        if (on_header_called > 2) {
            skip = 1;
        }
        const char* header = resp.headers[on_header_called + skip].c_str();
        size_t len = strlen(header);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, header, len) == 0 );
        ++on_header_called;
        return size;
    };

    istringstream iss = istringstream(resp.body);
    int on_write_called = 0;
    io_ctx.on_write = [=, &iss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        char body_buffer[size];
        memset(body_buffer, '\0', size);
        iss.read((char*)body_buffer, size);
        REQUIRE ( strncmp(buffer, body_buffer, size) == 0 );
        return size;
    };

    int on_close_called = 0;
    s_ctx.on_close = [=, &on_close_called](void* socket_ctx) {
        ++on_close_called;
        return KHC_SOCK_OK;
    };

    khc_code res = khc_perform(&http);
    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( on_connect_called == 1 );
    REQUIRE( on_send_called == 3 );
    REQUIRE( on_read_called == 0 );
    REQUIRE( on_recv_called == 7 );
    REQUIRE( on_header_called == 11 );
    REQUIRE( on_write_called > 0 );
    REQUIRE( on_close_called == 1 );
}

TEST_CASE( "HTTP response 400 test" ) {
    khc http;
    khc_init(&http);

    stringstream ss;
    ss << "HTTP/1.1 400 Bad Request\r\nContent-Length: 11\r\n\r\ndummy body.";

    khct::http::Resp resp(ss);

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    // no set cb_read.
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, uint16_t port) {
        ++on_connect_called;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    int on_send_called = 0;
    s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        ++on_send_called;
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    int on_read_called = 0;
    io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, void *userdata) {
        ++on_read_called;
        FAIL();
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
        if (on_recv_called == 1) {
            // recv header.
            REQUIRE( length_to_read == 255 );
        } else {
            // recv body(= 0).
            REQUIRE( length_to_read == 1024 );
        }
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    int on_header_called = 0;
    io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, void *userdata) {
        const char* header = resp.headers[on_header_called].c_str();
        size_t len = strlen(header);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, header, len) == 0 );
        ++on_header_called;
        return size;
    };

    istringstream iss = istringstream(resp.body);
    int on_write_called = 0;
    io_ctx.on_write = [=, &iss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        char body_buffer[size];
        memset(body_buffer, '\0', size);
        iss.read((char*)body_buffer, size);
        REQUIRE ( strncmp(buffer, body_buffer, size) == 0 );
        return size;
    };

    int on_close_called = 0;
    s_ctx.on_close = [=, &on_close_called](void* socket_ctx) {
        ++on_close_called;
        return KHC_SOCK_OK;
    };

    khc_code res = khc_perform(&http);
    REQUIRE( res == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 400 );
    REQUIRE( on_connect_called == 1 );
    REQUIRE( on_send_called == 3 );
    REQUIRE( on_read_called == 0 );
    REQUIRE( on_recv_called == 2 );
    REQUIRE( on_header_called == 2 );
    REQUIRE( on_write_called == 1 );
    REQUIRE( on_close_called == 1 );
}

namespace {

struct ContentLengthResult {
    khc_code res;
    size_t content_length;
    int on_header_called;
    int on_write_called;
    int on_close_called;
};

// Performs a GET whose response has the given header and a short body.
ContentLengthResult perform_with_header(const std::string& header) {
    khc http;
    khc_init(&http);

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    ContentLengthResult r = {};

    s_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return KHC_SOCK_OK;
    };
    s_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    istringstream is("HTTP/1.1 200 OK\r\n" + header + "\r\n\r\nbody");
    s_ctx.on_recv = [&is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };
    s_ctx.on_close = [&r](void* socket_ctx) {
        ++r.on_close_called;
        return KHC_SOCK_OK;
    };
    io_ctx.on_header = [&r](char *buffer, size_t size, void *userdata) {
        ++r.on_header_called;
        return size;
    };
    io_ctx.on_write = [&r](char *buffer, size_t size, void *userdata) {
        ++r.on_write_called;
        return size;
    };

    r.res = khc_perform(&http);
    r.content_length = http._resp_content_length;
    return r;
}

}

// Content-Length was read with strtol, so on a 32-bit target 2147483648 came
// back as LONG_MAX, and SIZE_MAX did on 32 and 64 bits alike.
TEST_CASE( "Content-Length is read into a size_t" ) {
    const struct {
        std::string value;
        size_t expected;
    } cases[] = {
        { "0", 0 },
        { "10", 10 },
        { "2147483648", (size_t)2147483648U },
        { std::to_string((size_t)SIZE_MAX), SIZE_MAX },
    };
    for (const auto& c : cases) {
        INFO("value " << c.value);
        ContentLengthResult r = perform_with_header("Content-Length: " + c.value);
        CHECK( r.res == KHC_ERR_OK );
        CHECK( r.content_length == c.expected );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 1 );
        CHECK( r.on_close_called == 1 );
    }
}

// With an invalid Content-Length the length of the body is unknown. The
// response fails as a header khc cannot handle, once the header callback has
// seen the header and before any of the body is written.
TEST_CASE( "Invalid Content-Length fails the response" ) {
    // SIZE_MAX is 2^n - 1, which never ends in 9.
    std::string over = std::to_string((size_t)SIZE_MAX);
    over.back() += 1;

    const std::string values[] = {
        "-1", "+5", "abc", "", "12abc", over, "123456789012345678901234567890"
    };
    for (const auto& value : values) {
        INFO("value [" << value << "]");
        ContentLengthResult r = perform_with_header("Content-Length: " + value);
        CHECK( r.res == KHC_ERR_HEADER_CALLBACK );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 0 );
        CHECK( r.on_close_called == 1 );
    }
}

// Only a header named Content-Length is one: a prefix of the name followed by
// a colon used to match, and would now fail the response.
TEST_CASE( "Header named like a prefix of Content-Length is not one" ) {
    const std::string headers[] = { "Content: abc", "Content-Len: -1" };
    for (const auto& header : headers) {
        INFO("header [" << header << "]");
        ContentLengthResult r = perform_with_header(header);
        CHECK( r.res == KHC_ERR_OK );
        CHECK( r.content_length == 0 );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 1 );
        CHECK( r.on_close_called == 1 );
    }
}

// A Transfer-Encoding whose final coding is not chunked leaves the body in a
// coding khc cannot remove. The response fails as a header khc cannot handle,
// once the header callback has seen the header and before any of the body is
// written. A value with chunked anywhere in it used to turn on chunked decoding.
TEST_CASE( "Transfer-Encoding not ending in chunked fails the response" ) {
    const std::string values[] = { "gzip", "chunked, gzip", "chunkedX" };
    for (const auto& value : values) {
        INFO("value [" << value << "]");
        ContentLengthResult r = perform_with_header("Transfer-Encoding: " + value);
        CHECK( r.res == KHC_ERR_HEADER_CALLBACK );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 0 );
        CHECK( r.on_close_called == 1 );
    }
}

// Only a header named Transfer-Encoding is one: a prefix of the name followed
// by a colon used to turn on chunked decoding of a body that is not chunked.
TEST_CASE( "Header named like a prefix of Transfer-Encoding is not one" ) {
    const std::string headers[] = { "T: chunked", "Transfer: chunked", ": chunked" };
    for (const auto& header : headers) {
        INFO("header [" << header << "]");
        ContentLengthResult r = perform_with_header(header);
        CHECK( r.res == KHC_ERR_OK );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 1 );
        CHECK( r.on_close_called == 1 );
    }
}
