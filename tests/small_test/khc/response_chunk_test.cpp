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
    // Binary because the fixtures are CRLF, as HTTP is. In text mode Windows
    // hands them back with LF line endings, and Resp, which splits headers on
    // CRLF, takes the whole file for a single header.
    ifs.open("./data/resp-login-chunked.txt", ios::binary);

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
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
    ifs.open("./data/resp-login-chunked.txt", ios::binary);

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
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
    ifs.open("./data/resp-login-chunked.txt", ios::binary);

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
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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
    ifs.open("./data/resp-login-chunked-headers.txt", ios::binary);

    khct::http::Resp resp(ifs);

    ifs.close();

    ostringstream responseBody;
    ostringstream expectBody;
    khct::http::create_random_chunked_body(responseBody, expectBody);
    resp.body = responseBody.str();

    // Binary too, so that on Windows the dump is byte for byte what khc is fed.
    ofstream ofs("tmp_random_chunk_response.txt", ios::binary);
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
    s_ctx.on_recv = [=, &on_recv_called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
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

namespace {

struct ChunkedResult {
    khc_code res;
    string body;
    int on_close_called;
};

// Performs a GET answered with the given chunked body. The socket hands over
// the headers in one read, as khc needs the status line in one, and the body
// at most max_recv bytes at a time, or as much as is asked for if 0.
ChunkedResult perform_chunked(const string& chunked_body, size_t max_recv) {
    khc http;
    khc_init(&http);

    // Stale bytes in the stream buffer are hex digits, so that a size read
    // past what was received shows.
    vector<char> stream_buff(DEFAULT_STREAM_BUFF_SIZE, 'f');
    khc_set_stream_buff(&http, stream_buff.data(), stream_buff.size());

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

    s_ctx.on_connect = [](void* socket_context, const char* host, unsigned int port) {
        return KHC_SOCK_OK;
    };
    s_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    const string headers = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
    istringstream is(headers + chunked_body);
    int on_recv_called = 0;
    s_ctx.on_recv = [&is, &on_recv_called, &headers, max_recv](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        // Far more reads than these responses need: khc is not making progress.
        if (++on_recv_called > 1000) {
            return KHC_SOCK_FAIL;
        }
        if (max_recv > 0) {
            size_t limit = on_recv_called == 1 ? headers.size() : max_recv;
            if (length_to_read > limit) {
                length_to_read = limit;
            }
        }
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };
    io_ctx.on_header = [](char *buffer, size_t size, void *userdata) {
        return size;
    };

    ChunkedResult result = { KHC_ERR_OK, "", 0 };
    io_ctx.on_write = [&result](char *buffer, size_t size, void *userdata) {
        result.body.append(buffer, size);
        return size;
    };
    s_ctx.on_close = [&result](void* socket_ctx) {
        ++result.on_close_called;
        return KHC_SOCK_OK;
    };

    result.res = khc_perform(&http);
    return result;
}

}

TEST_CASE( "chunk size lines in uppercase or with extensions" ) {
    // A byte at a time also splits the CRLF after each chunk's data.
    for (size_t max_recv : { 0, 1 }) {
        INFO("max_recv " << max_recv);
        ChunkedResult r = perform_chunked(
                "A;name=value\r\n0123456789\r\n"
                "0003 ;name=\"quoted value\"\r\nabc\r\n"
                "0;last\r\n\r\n",
                max_recv);
        REQUIRE( r.res == KHC_ERR_OK );
        REQUIRE( r.body == "0123456789abc" );
        REQUIRE( r.on_close_called == 1 );
    }
}

TEST_CASE( "an invalid chunk size line fails the response" ) {
    const string lines[] = {
        "-1",
        "+a",
        "xyz",
        "",
        "0x5",
        // One past SIZE_MAX, and 20 hex digits, too many for 64 bits.
        "1" + string(sizeof(size_t) * 2, '0'),
        "123456789abcdef01234",
        string("5\0", 2),
        string("5;\0", 3),
    };
    // The invalid size first, and after a chunk that is passed on.
    const struct {
        string chunks;
        string body;
    } befores[] = {
        { "", "" },
        { "3\r\nabc\r\n", "abc" },
    };
    for (const auto& line : lines) {
        for (const auto& before : befores) {
            for (size_t max_recv : { 0, 1 }) {
                INFO("line \"" << line << "\" after \"" << before.chunks << "\", max_recv " << max_recv);
                ChunkedResult r = perform_chunked(
                        before.chunks + line + "\r\nhello\r\n0\r\n\r\n",
                        max_recv);
                REQUIRE( r.res == KHC_ERR_FAIL );
                REQUIRE( r.body == before.body );
                REQUIRE( r.on_close_called == 1 );
            }
        }
    }
}
