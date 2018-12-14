#include "catch.hpp"
#include <khc.h>
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <fstream>
#include <iostream>
#include <random>
#include <string.h>

TEST_CASE( "HTTP chunked response test" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;

    ifstream ifs;
    ifs.open("./data/resp-login-chunked.txt");

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
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port) {
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
        REQUIRE( size == buff_size );
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
        if (on_recv_called == 1)
            REQUIRE( length_to_read == 255 );
        if (on_recv_called == 2)
            REQUIRE( length_to_read == 236 );
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

    ostringstream oss;
    int on_write_called = 0;
    io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
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
    REQUIRE( on_send_called == 5 );
    REQUIRE( on_read_called == 1 );
    REQUIRE( on_recv_called == 4 );
    REQUIRE( on_header_called == 12 );
    REQUIRE( on_write_called >= 6 );
    REQUIRE( on_close_called == 1 );

    string chunkedBody =
        "{"
        "  \"id\" : \"b56270b00022-171b-7e11-b35e-0911a10d\","
        "  \"access_token\" : \"cHltZmFtc3cxMnJn._I4i4fNNTjRfWia9juCpRgipPUNWD1-6bobnfJvQPms\","
        "  \"expires_in\" : 2147483646,"
        "  \"token_type\" : \"Bearer\""
        "}";
    REQUIRE( chunkedBody == oss.str() );
}

TEST_CASE( "small buffer size test" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = 10;
    char buff[buff_size];

    ifstream ifs;
    ifs.open("./data/resp-login-chunked.txt");

    khct::http::Resp resp(ifs);

    ifs.close();

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_stream_buff(&http, buff, buff_size);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port) {
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
        REQUIRE( size == buff_size );
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
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

    ostringstream oss;
    int on_write_called = 0;
    io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
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
    REQUIRE( on_send_called == 5 );
    REQUIRE( on_read_called == 1 );
    REQUIRE( on_recv_called > 0 );
    REQUIRE( on_header_called == 12 );
    REQUIRE( on_write_called > 0 );
    REQUIRE( on_close_called == 1 );

    string chunkedBody =
        "{"
        "  \"id\" : \"b56270b00022-171b-7e11-b35e-0911a10d\","
        "  \"access_token\" : \"cHltZmFtc3cxMnJn._I4i4fNNTjRfWia9juCpRgipPUNWD1-6bobnfJvQPms\","
        "  \"expires_in\" : 2147483646,"
        "  \"token_type\" : \"Bearer\""
        "}";
    REQUIRE( chunkedBody == oss.str() );
}

TEST_CASE( "random buffer size test" ) {
    khc http;
    khc_init(&http);
    random_device rd;
    mt19937 mt(rd());
    uniform_int_distribution<> randSize(10, 100);
    const size_t buff_size = randSize(mt);
    char buff[buff_size];
    cout << "Random Buffer Size: " << buff_size << endl;

    ifstream ifs;
    ifs.open("./data/resp-login-chunked.txt");

    khct::http::Resp resp(ifs);

    ifs.close();

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_stream_buff(&http, buff, buff_size);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port) {
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
        REQUIRE( size == buff_size );
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
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

    ostringstream oss;
    int on_write_called = 0;
    io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
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
    REQUIRE( on_send_called == 5 );
    REQUIRE( on_read_called == 1 );
    REQUIRE( on_recv_called > 0 );
    REQUIRE( on_header_called == 12 );
    REQUIRE( on_write_called > 0 );
    REQUIRE( on_close_called == 1 );

    string chunkedBody =
        "{"
        "  \"id\" : \"b56270b00022-171b-7e11-b35e-0911a10d\","
        "  \"access_token\" : \"cHltZmFtc3cxMnJn._I4i4fNNTjRfWia9juCpRgipPUNWD1-6bobnfJvQPms\","
        "  \"expires_in\" : 2147483646,"
        "  \"token_type\" : \"Bearer\""
        "}";
    REQUIRE( chunkedBody == oss.str() );
}

TEST_CASE( "random chunk body test" ) {
    khc http;
    khc_init(&http);

    ifstream ifs;
    ifs.open("./data/resp-login-chunked-headers.txt");

    khct::http::Resp resp(ifs);

    ifs.close();

    ostringstream responseBody;
    ostringstream expectBody;
    khct::http::create_random_chunked_body(responseBody, expectBody);
    resp.body = responseBody.str();

    ofstream ofs("tmp_random_chunk_response.txt");
    ofs << resp.to_string();
    ofs.close();

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
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    int on_connect_called = 0;
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port) {
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
        REQUIRE( size == DEFAULT_STREAM_BUFF_SIZE);
        return 0;
    };

    int on_recv_called = 0;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        ++on_recv_called;
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

    ostringstream oss;
    int on_write_called = 0;
    io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
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
    REQUIRE( on_send_called > 0 );
    REQUIRE( on_read_called > 0 );
    REQUIRE( on_recv_called > 0 );
    REQUIRE( on_header_called == 12 );
    REQUIRE( on_write_called > 0 );
    REQUIRE( on_close_called == 1 );

    REQUIRE( expectBody.str() == oss.str() );
}

TEST_CASE( "HTTP response 400 test(chunked)" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;

    string expectBody = "dummy body.";
    stringstream ss;
    ss << "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n";
    ss << hex << expectBody.size() << "\r\n" << expectBody << "\r\n0\r\n\r\n";

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
    s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port) {
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
    s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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

    ostringstream oss;
    int on_write_called = 0;
    io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, void *userdata) {
        ++on_write_called;
        oss.write(buffer, size);
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

    REQUIRE( expectBody == oss.str() );
}
