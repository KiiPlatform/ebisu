#include "catch.hpp"
#include <khc.h>
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <fstream>
#include <functional>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

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

// Performs a request whose response has the given header and a short body.
ContentLengthResult perform_with_header(
        const std::string& header,
        const std::string& status = "200 OK",
        const std::string& method = "GET") {
    khc http;
    khc_init(&http);

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, method.c_str());
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
    istringstream is("HTTP/1.1 " + status + "\r\n" + header + "\r\n\r\nbody");
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
        { "4", 4 },
        { "10", 10 },
        { "2147483648", (size_t)2147483648U },
        { std::to_string((size_t)SIZE_MAX), SIZE_MAX },
    };
    for (const auto& c : cases) {
        INFO("value " << c.value);
        ContentLengthResult r = perform_with_header("Content-Length: " + c.value);
        // The body is 4 bytes.
        CHECK( r.res == (c.expected > 4 ? KHC_ERR_SOCK_RECV : KHC_ERR_OK) );
        CHECK( r.content_length == c.expected );
        CHECK( r.on_header_called == 2 );
        CHECK( r.on_write_called == 1 );
        CHECK( r.on_close_called == 1 );
    }
}

// A response is delimited by the connection closing, so one cut short looked
// complete.
TEST_CASE( "A body shorter than its Content-Length fails the response" ) {
    const std::string statuses[] = { "200 OK", "404 Not Found", "500 Internal Server Error" };
    for (const auto& status : statuses) {
        INFO("status " << status);
        ContentLengthResult r = perform_with_header("Content-Length: 5", status);
        CHECK( r.res == KHC_ERR_SOCK_RECV );
        CHECK( r.on_write_called == 1 );
        CHECK( r.on_close_called == 1 );
    }
}

TEST_CASE( "A body as long as its Content-Length, or longer, is a response" ) {
    for (int length : { 3, 4 }) {
        INFO("length " << length);
        ContentLengthResult r = perform_with_header("Content-Length: " + std::to_string(length));
        CHECK( r.res == KHC_ERR_OK );
        CHECK( r.on_write_called == 1 );
    }
}

// These give the length a body would have had, and don't send one.
TEST_CASE( "Content-Length of a response with no body" ) {
    const std::string length = "Content-Length: 100";

    SECTION("HEAD") {
        CHECK( perform_with_header(length, "200 OK", "HEAD").res == KHC_ERR_OK );
    }
    SECTION("204") {
        CHECK( perform_with_header(length, "204 No Content").res == KHC_ERR_OK );
    }
    SECTION("304") {
        CHECK( perform_with_header(length, "304 Not Modified").res == KHC_ERR_OK );
    }
}

TEST_CASE( "Content-Length of an interim response is not the final one's" ) {
    ContentLengthResult r = perform_with_header(
            "Content-Length: 100\r\n\r\nHTTP/1.1 200 OK", "100 Continue");
    CHECK( r.res == KHC_ERR_OK );
    CHECK( r.content_length == 0 );
}

namespace {

// Performs a GET whose response comes in the given reads.
khc_code perform_with_reads(const std::vector<std::string>& reads, uint16_t* status_code) {
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

    s_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return KHC_SOCK_OK;
    };
    s_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    size_t next = 0;
    s_ctx.on_recv = [&reads, &next](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = 0;
        if (next < reads.size()) {
            *out_actual_length = reads[next].copy(buffer, length_to_read);
            ++next;
        }
        return KHC_SOCK_OK;
    };
    s_ctx.on_close = [](void* socket_ctx) {
        return KHC_SOCK_OK;
    };
    io_ctx.on_header = [](char *buffer, size_t size, void *userdata) {
        return size;
    };
    io_ctx.on_write = [](char *buffer, size_t size, void *userdata) {
        return size;
    };

    khc_code res = khc_perform(&http);
    *status_code = khc_get_status_code(&http);
    return res;
}

}

// A read returns what has arrived, wherever that ends. Taking one that held
// part of the first line for a line too long failed the request.
TEST_CASE( "A status line arriving in several reads" ) {
    const std::string response = "HTTP/1.1 200 OK\r\n\r\nbody";
    const size_t line_size = strlen("HTTP/1.1 200 OK\r\n");
    for (size_t split = 1; split < line_size; ++split) {
        INFO("split at " << split);
        uint16_t status_code = 0;
        khc_code res = perform_with_reads(
                { response.substr(0, split), response.substr(split) }, &status_code);
        CHECK( res == KHC_ERR_OK );
        CHECK( status_code == 200 );
    }

    uint16_t status_code = 0;
    std::vector<std::string> bytes;
    for (char c : response) {
        bytes.push_back(std::string(1, c));
    }
    CHECK( perform_with_reads(bytes, &status_code) == KHC_ERR_OK );
    CHECK( status_code == 200 );
}

TEST_CASE( "A connection closed before the status line ended" ) {
    const std::vector<std::string> reads_list[] = {
        {},
        { "HTTP/1.1 20" },
        { "HTTP/1.1 200 OK" },
        { "HTTP/1.1 ", "200 OK\r" },
    };
    for (const auto& reads : reads_list) {
        INFO("first read of " << reads.size());
        uint16_t status_code = 0;
        CHECK( perform_with_reads(reads, &status_code) == KHC_ERR_SOCK_RECV );
        CHECK( status_code == 0 );
    }
}

TEST_CASE( "A first line that isn't a status line" ) {
    const std::string lines[] = {
        "\r\n",
        "OK\r\n",
        "HTTP/1.1\r\n",
        "HTTP/1.1 20\r\n",
        "FTP/1.1 200 OK\r\n",
        "        200 OK\r\n",
    };
    for (const auto& line : lines) {
        INFO("line [" << line << "]");
        uint16_t status_code = 0;
        CHECK( perform_with_reads({ line + "\r\nbody" }, &status_code) == KHC_ERR_FAIL );
        CHECK( status_code == 0 );
    }
}

TEST_CASE( "A status line without a reason phrase" ) {
    uint16_t status_code = 0;
    CHECK( perform_with_reads({ "HTTP/1.1 204\r\n\r\n" }, &status_code) == KHC_ERR_OK );
    CHECK( status_code == 204 );
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

namespace {

// Reads, in a test that has gone wrong, before it stops: closing is meant to
// end the request on the first read that finds it.
const int MAX_RECV_CALLS = 32;

struct ClosedResult {
    khc_code res;
    int on_recv_called;
    int on_close_called;
};

// Performs a GET whose response is the given bytes, then the connection
// closing.
ClosedResult perform_closing_after(const std::string& response) {
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

    ClosedResult r = {};

    s_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return KHC_SOCK_OK;
    };
    s_ctx.on_send = [](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    istringstream is(response);
    s_ctx.on_recv = [&is, &r](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        if (++r.on_recv_called >= MAX_RECV_CALLS) {
            return KHC_SOCK_FAIL;
        }
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };
    s_ctx.on_close = [&r](void* socket_ctx) {
        ++r.on_close_called;
        return KHC_SOCK_OK;
    };
    io_ctx.on_header = [](char *buffer, size_t size, void *userdata) {
        return size;
    };
    io_ctx.on_write = [](char *buffer, size_t size, void *userdata) {
        return size;
    };

    r.res = khc_perform(&http);
    return r;
}

}

// However far a response got, a connection that closes before it ends fails
// the request as a failure to receive, on the first read that finds it closed.
TEST_CASE( "A connection closed before the response ended" ) {
    const std::string status_line = "HTTP/1.1 200 OK\r\n";
    const std::string chunked = status_line + "Transfer-Encoding: chunked\r\n\r\n";
    const std::string responses[] = {
        // In the headers.
        status_line,
        status_line + "Content-Le",
        status_line + "Content-Length: 4\r\n",
        // In a header longer than the response header buffer.
        status_line + "X-Long: " + std::string(1000, 'a'),
        // In a chunked body.
        chunked,
        chunked + "4",
        chunked + "4\r\nbo",
        chunked + "4\r\nbody",
        chunked + "4\r\nbody\r",
    };
    for (const auto& response : responses) {
        INFO("response [" << response.substr(0, 80) << "]");
        ClosedResult r = perform_closing_after(response);
        CHECK( r.res == KHC_ERR_SOCK_RECV );
        CHECK( r.on_recv_called < MAX_RECV_CALLS );
        CHECK( r.on_close_called == 1 );
    }
}

TEST_CASE( "A response ended by the connection closing" ) {
    ClosedResult r = perform_closing_after("HTTP/1.1 200 OK\r\n\r\nbody");
    CHECK( r.res == KHC_ERR_OK );
    CHECK( r.on_recv_called < MAX_RECV_CALLS );
    CHECK( r.on_close_called == 1 );
}

namespace {

struct HeaderResult {
    khc_code res;
    bool skipped;
    std::vector<std::string> headers;
    std::string body;
};

// Performs a GET answered with resp, read through a header buffer of 64 bytes.
HeaderResult perform_with_small_header_buffer(const std::string& resp)
{
    char resp_header_buff[64];
    khc http;
    khc_init(&http);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_resp_header_buff(&http, resp_header_buff, sizeof(resp_header_buff));

    size_t resp_pos = 0;
    khct::cb::SockCtx s_ctx;
    s_ctx.on_connect = [](void*, const char*, uint16_t) { return KHC_SOCK_OK; };
    s_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    s_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.copy(buffer, length_to_read, resp_pos);
        resp_pos += *out_actual_length;
        return KHC_SOCK_OK;
    };
    s_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    HeaderResult result;
    khct::cb::IOCtx io_ctx;
    io_ctx.on_header = [&result](char* buffer, size_t size, void*) {
        result.headers.push_back(std::string(buffer, size));
        return size;
    };
    io_ctx.on_write = [&result](char* buffer, size_t size, void*) {
        result.body.append(buffer, size);
        return size;
    };
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    result.res = khc_perform(&http);
    result.skipped = khc_get_resp_header_skipped(&http);
    return result;
}

}

TEST_CASE( "A header too long for the buffer is skipped, and says so" ) {
    HeaderResult r = perform_with_small_header_buffer(
            "HTTP/1.1 200 OK\r\n"
            "Set-Cookie: " + std::string(200, 'c') + "\r\n"
            "X-Kept: yes\r\n"
            "Content-Length: 4\r\n"
            "\r\n"
            "body");
    CHECK( r.res == KHC_ERR_OK );
    CHECK( r.skipped == true );
    CHECK( r.body == "body" );
    // The status line, then what fitted.
    REQUIRE( r.headers.size() == 3 );
    CHECK( r.headers[1] == "X-Kept: yes" );
    CHECK( r.headers[2] == "Content-Length: 4" );
}

TEST_CASE( "Headers that fit skip nothing" ) {
    HeaderResult r = perform_with_small_header_buffer(
            "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nbody");
    CHECK( r.res == KHC_ERR_OK );
    CHECK( r.skipped == false );
    CHECK( r.body == "body" );
}

// No callback failed, so KHC_ERR_HEADER_CALLBACK sent the reader the wrong way.
// The buffer is what to change.
TEST_CASE( "A framing header too long for the buffer is too large" ) {
    const std::string spaces(100, ' ');
    const char* framing[] = { "Content-Length:", "Transfer-Encoding:" };
    for (const char* name : framing) {
        INFO(name);
        HeaderResult r = perform_with_small_header_buffer(
                std::string("HTTP/1.1 200 OK\r\n") + name + spaces + "4\r\n\r\nbody");
        CHECK( r.res == KHC_ERR_TOO_LARGE_DATA );
        CHECK( r.skipped == true );
    }
}

// Each was called without checking, so one left out crashed the request.
TEST_CASE( "khc_perform needs its callbacks" ) {
    const struct {
        const char* left_out;
        std::function<void(khc*)> unset;
    } cases[] = {
        { "connect", [](khc* h) { khc_set_cb_sock_connect(h, NULL, NULL); } },
        { "send", [](khc* h) { khc_set_cb_sock_send(h, NULL, NULL); } },
        { "recv", [](khc* h) { khc_set_cb_sock_recv(h, NULL, NULL); } },
        { "close", [](khc* h) { khc_set_cb_sock_close(h, NULL, NULL); } },
        { "write", [](khc* h) { khc_set_cb_write(h, NULL, NULL); } },
        { "header", [](khc* h) { khc_set_cb_header(h, NULL, NULL); } },
    };
    for (const auto& c : cases) {
        INFO("left out " << c.left_out);
        khc http;
        khc_init(&http);
        khc_set_host(&http, "api.kii.com");
        khc_set_method(&http, "GET");
        khc_set_path(&http, "/api/apps");
        khc_set_req_headers(&http, NULL);

        int on_connect_called = 0;
        khct::cb::SockCtx s_ctx;
        s_ctx.on_connect = [&on_connect_called](void*, const char*, uint16_t) {
            ++on_connect_called;
            return KHC_SOCK_FAIL;
        };
        khct::cb::IOCtx io_ctx;
        khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
        khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
        khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
        khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);
        khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
        khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

        c.unset(&http);
        CHECK( khc_perform(&http) == KHC_ERR_FAIL );
        CHECK( on_connect_called == 0 );
    }
}

// A request with no body has nothing to read.
TEST_CASE( "khc_perform does without the read callback" ) {
    HeaderResult r = perform_with_small_header_buffer(
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    CHECK( r.res == KHC_ERR_OK );
}
