#define CATCH_CONFIG_MAIN

#include <string.h>
#include "catch.hpp"
#include <khc.h>
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <sstream>

TEST_CASE( "HTTP minimal" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    khct::http::Resp resp;
    resp.headers = { "HTTP/1.0 200 OK" };

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

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "GET https://api.kii.com/api/apps HTTP/1.1\r\n";
        REQUIRE( length == strlen(req_line) );
        REQUIRE( strncmp(buffer, req_line, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char host_hdr[] = "HOST: api.kii.com\r\n";
        REQUIRE( length == strlen(host_hdr) );
        REQUIRE( strncmp(buffer, host_hdr, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_host_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_req_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_END );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        REQUIRE( length == 40 );
        REQUIRE( strncmp(buffer, "Content-Length: 0\r\nConnection: Close\r\n\r\n", 40) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    khc_state_req_header_end(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    char buffer[resp_header_buff_size];
    size_t len = resp.to_istringstream().read((char*)&buffer, resp_header_buff_size - 1).gcount();
    REQUIRE( http._resp_header_read_size == len );
    REQUIRE( called );

    khc_state_resp_status_parse(&http);
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );

    called = false;
    io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, void *userdata) {
        called = true;
        const char* status_line = resp.headers[0].c_str();
        size_t len = strlen(status_line);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, status_line, len) == 0 );
        return size;
    };

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_FRAGMENT );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_fragment(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == buff_size);
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };
    khc_state_resp_body_read(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == 1 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return KHC_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}

TEST_CASE( "HTTP 1.1 chunked minimal" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    khct::http::Resp resp;
    resp.headers = {
        "HTTP/1.1 200 OK",
        "Host: api.kii.com",
        "Transfer-Encoding: chunked",
    };
    resp.body = "0\r\n\r\n";

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

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "GET https://api.kii.com/api/apps HTTP/1.1\r\n";
        REQUIRE( length == strlen(req_line) );
        REQUIRE( strncmp(buffer, req_line, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char host_hdr[] = "HOST: api.kii.com\r\n";
        REQUIRE( length == strlen(host_hdr) );
        REQUIRE( strncmp(buffer, host_hdr, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_host_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_req_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_END );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        REQUIRE( length == 49 );
        REQUIRE( strncmp(buffer, "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n", 49) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    khc_state_req_header_end(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_read = [=, &called](char *buffer, size_t size, void *userdata) {
        called = true;
        REQUIRE( size == buff_size );
        const char body[] = "http body";
        strncpy(buffer, body, strlen(body));
        return strlen(body);
    };
    khc_state_req_body_read(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._read_req_end == 0 );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "9\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_size(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "http body";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_crlf(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_read = [=, &called](char *buffer, size_t size, void *userdata) {
        called = true;
        REQUIRE( size == buff_size );
        return 0;
    };
    khc_state_req_body_read(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._read_req_end == 1 );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "0\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_size(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_crlf(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    char buffer[resp_header_buff_size];
    size_t len = resp.to_istringstream().read((char*)&buffer, resp_header_buff_size - 1).gcount();
    REQUIRE( http._resp_header_read_size == len );
    REQUIRE( called );

    khc_state_resp_status_parse(&http);
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );

    called = false;
    io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, void *userdata) {
        called = true;
        const char* status_line = resp.headers[0].c_str();
        size_t len = strlen(status_line);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, status_line, len) == 0 );
        return size;
    };

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
    REQUIRE( http._chunked_resp == 0 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, void *userdata) {
        called = true;
        const char* host_line = resp.headers[1].c_str();
        size_t len = strlen(host_line);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, host_line, len) == 0 );
        return size;
    };

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
    REQUIRE( http._chunked_resp == 0 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, void *userdata) {
        called = true;
        const char* chunked_line = resp.headers[2].c_str();
        size_t len = strlen(chunked_line);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, chunked_line, len) == 0 );
        return size;
    };

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 0 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_parse_chunk_size(&http);
    REQUIRE( http._state == KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 0 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_read_chunk_size_from_header_buff(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 5 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_parse_chunk_size(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_SKIP_TRAILERS );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 2 );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        *out_actual_length = 0;
        return KHC_SOCK_OK;
    };
    khc_state_resp_body_skip_trailers(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return KHC_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}

TEST_CASE( "Socket send partial" ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    khct::http::Resp resp;
    resp.headers = { "HTTP/1.0 200 OK" };

    char req_header[] = "X-KII-dummy-Header: test partial.";
    khc_slist* req_headers = NULL;
    req_headers = khc_slist_append(req_headers, req_header, strlen(req_header));

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, req_headers);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return KHC_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "GET https://api.kii.";
        size_t req_len = strlen(req_line);
        REQUIRE( length > req_len );
        REQUIRE( strncmp(buffer, req_line, req_len) == 0 );
        *out_sent_length = req_len;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "com/api/apps HT";
        size_t req_len = strlen(req_line);
        REQUIRE( length > req_len );
        REQUIRE( strncmp(buffer, req_line, req_len) == 0 );
        *out_sent_length = req_len;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "TP/1.1\r\n";
        size_t req_len = strlen(req_line);
        REQUIRE( length == req_len );
        REQUIRE( strncmp(buffer, req_line, req_len) == 0 );
        *out_sent_length = req_len;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char host_hdr[] = "HOST: api.";
        size_t host_len = strlen(host_hdr);
        REQUIRE( length > host_len );
        REQUIRE( strncmp(buffer, host_hdr, host_len) == 0 );
        *out_sent_length = host_len;
        return KHC_SOCK_OK;
    };
    khc_state_req_host_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char host_hdr[] = "kii.com\r\n";
        size_t host_len = strlen(host_hdr);
        REQUIRE( length == host_len );
        REQUIRE( strncmp(buffer, host_hdr, host_len) == 0 );
        *out_sent_length = host_len;
        return KHC_SOCK_OK;
    };
    khc_state_req_host_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_req_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "X-KII-dummy-";
        size_t len = strlen(hdr);
        REQUIRE( length > len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };
    khc_state_req_header_send(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "Header: test p";
        size_t len = strlen(hdr);
        REQUIRE( length > len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };
    khc_state_req_header_send(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "artial.";
        size_t len = strlen(hdr);
        REQUIRE( length == len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };
    khc_state_req_header_send(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND_CRLF );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "\r\n";
        size_t len = strlen(hdr);
        REQUIRE( length == len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };
    khc_state_req_header_send_crlf(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_req_header(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_END );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "Transfer-Encoding: chunked\r\n";
        size_t len = strlen(hdr);
        REQUIRE( length > len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };

    khc_state_req_header_end(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HEADER_END );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char hdr[] = "Connection: Close\r\n\r\n";
        size_t len = strlen(hdr);
        REQUIRE( length == len );
        REQUIRE( strncmp(buffer, hdr, len) == 0 );
        *out_sent_length = len;
        return KHC_SOCK_OK;
    };

    khc_state_req_header_end(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_read = [=, &called](char *buffer, size_t size, void *userdata) {
        called = true;
        REQUIRE( size == buff_size );
        const char body[] = "http body";
        strncpy(buffer, body, strlen(body));
        return strlen(body);
    };
    khc_state_req_body_read(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._read_req_end == 0 );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "9\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_size(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "http body";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_crlf(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    io_ctx.on_read = [=, &called](char *buffer, size_t size, void *userdata) {
        called = true;
        REQUIRE( size == buff_size );
        return 0;
    };
    khc_state_req_body_read(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._read_req_end == 1 );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "0\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_size(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char body[] = "\r\n";
        REQUIRE( length == strlen(body) );
        REQUIRE( strncmp(buffer, body, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };
    khc_state_req_body_send_crlf(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    char buffer[resp_header_buff_size];
    size_t len = resp.to_istringstream().read((char*)&buffer, resp_header_buff_size - 1).gcount();
    REQUIRE( http._resp_header_read_size == len );
    REQUIRE( called );

    khc_state_resp_status_parse(&http);
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );

    called = false;
    io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, void *userdata) {
        called = true;
        const char* status_line = resp.headers[0].c_str();
        size_t len = strlen(status_line);
        REQUIRE( size == len );
        REQUIRE( strncmp(buffer, status_line, len) == 0 );
        return size;
    };

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_FRAGMENT );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_fragment(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
    REQUIRE( http._read_end == 0 );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == buff_size);
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return KHC_SOCK_OK;
    };
    khc_state_resp_body_read(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == 1 );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return KHC_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_slist_free_all(req_headers);
}

TEST_CASE( "state abnormal tests." ) {
    khc http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;
    char stream_buff[buff_size];
    char resp_header_buff[resp_header_buff_size];

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_stream_buff(&http, stream_buff, buff_size);
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

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called;
    SECTION("state connect retry.") {
        called = false;
        s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_connect(&http);
        REQUIRE( http._state == KHC_STATE_CONNECT );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state connect failed.") {
        called = false;
        s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_connect(&http);
        REQUIRE( http._state == KHC_STATE_FINISHED );
        REQUIRE( http._result == KHC_ERR_SOCK_CONNECT );
        REQUIRE( called );
    }

    SECTION("state req line retry.") {
        http._state = KHC_STATE_REQ_LINE;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_line(&http);
        REQUIRE( http._state == KHC_STATE_REQ_LINE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req line failed.") {
        http._state = KHC_STATE_REQ_LINE;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_line(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req host header retry.") {
        http._state = KHC_STATE_REQ_HOST_HEADER;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_host_header(&http);
        REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req host header failed.") {
        http._state = KHC_STATE_REQ_HOST_HEADER;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_host_header(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req header send retry.") {
        http._state = KHC_STATE_REQ_HEADER_SEND;
        http._sent_length = 0;
        khc_slist header;
        header.data = (char*)"dummy";
        header.next = NULL;
        http._current_req_header = &header;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_header_send(&http);
        REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req header send failed.") {
        http._state = KHC_STATE_REQ_HEADER_SEND;
        http._sent_length = 0;
        khc_slist header;
        header.data = (char*)"dummy";
        header.next = NULL;
        http._current_req_header = &header;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_header_send(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req header send crlf retry.") {
        http._state = KHC_STATE_REQ_HEADER_SEND_CRLF;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_header_send_crlf(&http);
        REQUIRE( http._state == KHC_STATE_REQ_HEADER_SEND_CRLF );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req header send crlf failed.") {
        http._state = KHC_STATE_REQ_HEADER_SEND_CRLF;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_header_send_crlf(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req header end retry.") {
        http._state = KHC_STATE_REQ_HEADER_END;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_header_end(&http);
        REQUIRE( http._state == KHC_STATE_REQ_HEADER_END );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req header end failed.") {
        http._state = KHC_STATE_REQ_HEADER_END;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_header_end(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req body send size retry.") {
        http._state = KHC_STATE_REQ_BODY_SEND_SIZE;
        http._read_size = 1234;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_body_send_size(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req body send size failed.") {
        http._state = KHC_STATE_REQ_BODY_SEND_SIZE;
        http._read_size = 1234;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_body_send_size(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req body send retry.") {
        http._state = KHC_STATE_REQ_BODY_SEND;
        http._read_size = 1234;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req body send failed.") {
        http._state = KHC_STATE_REQ_BODY_SEND;
        http._read_size = 1234;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("state req body send crlf retry.") {
        http._state = KHC_STATE_REQ_BODY_SEND_CRLF;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_req_body_send_crlf(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req body send crlf failed.") {
        http._state = KHC_STATE_REQ_BODY_SEND_CRLF;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_req_body_send_crlf(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("resp status read no crlf.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            strcpy(buffer, "no crlf");
            *out_actual_length = strlen(buffer);
            return KHC_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
        REQUIRE( called );
    }

    SECTION("resp status read retry.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp status read failed.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp status parse no number.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            strcpy(buffer, "HTTP/1.1 abc no number status\r\n");
            *out_actual_length = strlen(buffer);
            return KHC_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );

        khc_state_resp_status_parse(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_FAIL );
    }

    SECTION("resp header read retry.") {
        http._state = KHC_STATE_RESP_HEADER_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_header_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_HEADER_READ );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp header read failed.") {
        http._state = KHC_STATE_RESP_HEADER_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_header_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp header callback failed.") {
        http._state = KHC_STATE_RESP_HEADER_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            strcpy(buffer, "X-HEADER: dummy value\r\n");
            *out_actual_length = strlen(buffer);
            return KHC_SOCK_OK;
        };

        khc_state_resp_header_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );

        called = false;
        io_ctx.on_header = [=, &called](char *buffer, size_t size, void *userdata) {
            called = true;
            REQUIRE( size > 0 );
            return 0;
        };

        khc_state_resp_header_callback(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_HEADER_CALLBACK );
        REQUIRE( called );
    }

    SECTION("resp header skip retry.") {
        http._state = KHC_STATE_RESP_HEADER_SKIP;
        char sbuff[15];
        strcpy(sbuff, "X-Dummy: dummy");
        khc_set_resp_header_buff(&http, sbuff, 14);
        http._resp_header_read_size = 14;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_header_skip(&http);
        REQUIRE( http._state == KHC_STATE_RESP_HEADER_SKIP );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp header skip failed.") {
        http._state = KHC_STATE_RESP_HEADER_SKIP;
        char sbuff[15];
        strcpy(sbuff, "X-Dummy: dummy");
        khc_set_resp_header_buff(&http, sbuff, 14);
        http._resp_header_read_size = 14;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_header_skip(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp header skip failed by Content-Length.") {
        http._state = KHC_STATE_RESP_HEADER_SKIP;
        char sbuff[20];
        strcpy(sbuff, "Content-Length: 200");
        khc_set_resp_header_buff(&http, sbuff, 19);
        http._resp_header_read_size = 19;

        khc_state_resp_header_skip(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_HEADER_CALLBACK );
    }

    SECTION("resp header skip failed by Transfer-Encoding.") {
        http._state = KHC_STATE_RESP_HEADER_SKIP;
        char sbuff[27];
        strcpy(sbuff, "Transfer-Encoding: chunked");
        khc_set_resp_header_buff(&http, sbuff, 26);
        http._resp_header_read_size = 26;

        khc_state_resp_header_skip(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_HEADER_CALLBACK );
    }

    SECTION("resp body fragment failed.") {
        http._state = KHC_STATE_RESP_BODY_FRAGMENT;
        http._resp_header_read_size = 10;

        called = false;
        io_ctx.on_write = [=, &called](char *buffer, size_t size, void *userdata) {
            called = true;
            REQUIRE( size > 0 );
            return 0;
        };

        khc_state_resp_body_fragment(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_WRITE_CALLBACK );
        REQUIRE( called );
    }

    SECTION("read chunk size from header buff failed.") {
        http._state = KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF;
        char sbuff[10];
        khc_set_stream_buff(&http, sbuff, 10);
        http._body_read_size = 256;

        khc_state_read_chunk_size_from_header_buff(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
    }

    SECTION("read chunk body from header buff failed.") {
        http._state = KHC_STATE_READ_CHUNK_BODY_FROM_HEADER_BUFF;
        char sbuff[10];
        khc_set_stream_buff(&http, sbuff, 10);
        http._body_read_size = 256;

        khc_state_read_chunk_body_from_header_buff(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
    }

    SECTION("resp body read retry.") {
        http._state = KHC_STATE_RESP_BODY_READ;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_body_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp body read failed.") {
        http._state = KHC_STATE_RESP_BODY_READ;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_body_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body callback failed.") {
        http._state = KHC_STATE_RESP_BODY_CALLBACK;
        http._body_read_size = 10;

        called = false;
        io_ctx.on_write = [=, &called](char *buffer, size_t size, void *userdata) {
            called = true;
            REQUIRE( size > 0 );
            return 0;
        };

        khc_state_resp_body_callback(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_WRITE_CALLBACK );
        REQUIRE( called );
    }

    SECTION("resp body read chunk size retry.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_body_read_chunk_size(&http);
        REQUIRE( http._state == KHC_STATE_RESP_BODY_READ_CHUNK_SIZE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp body read chunk size failed.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_body_read_chunk_size(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body read chunk size no buffer space.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;
        char sbuff[10];
        khc_set_stream_buff(&http, sbuff, 10);
        http._body_read_size = 256;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            *out_actual_length = length_to_read;
            return KHC_SOCK_OK;
        };

        khc_state_resp_body_read_chunk_size(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
        REQUIRE( called == false );
    }

    SECTION("resp body parse chunk body failed.") {
        http._state = KHC_STATE_RESP_BODY_PARSE_CHUNK_BODY;
        http._chunk_size = 10;
        http._body_read_size = 10;
        http._chunk_size_written = 0;

        called = false;
        io_ctx.on_write = [=, &called](char *buffer, size_t size, void *userdata) {
            called = true;
            REQUIRE( size > 0 );
            return 0;
        };

        khc_state_resp_body_parse_chunk_body(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_WRITE_CALLBACK );
        REQUIRE( called );
    }

    SECTION("resp body read chunk body retry.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_BODY;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_body_read_chunk_body(&http);
        REQUIRE( http._state == KHC_STATE_RESP_BODY_READ_CHUNK_BODY );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp body read chunk body failed.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_BODY;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_body_read_chunk_body(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body read chunk body failed by recv 0.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_BODY;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            *out_actual_length = 0;
            return KHC_SOCK_OK;
        };

        khc_state_resp_body_read_chunk_body(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_FAIL );
        REQUIRE( called );
    }

    SECTION("resp body skip chunk body crlf retry.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF;
        http._body_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_body_skip_chunk_body_crlf(&http);
        REQUIRE( http._state == KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp body skip chunk body crlf failed.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF;
        http._body_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_body_skip_chunk_body_crlf(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body skip chunk body crlf failed by recv 0.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF;
        http._body_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            *out_actual_length = 0;
            return KHC_SOCK_OK;
        };

        khc_state_resp_body_skip_chunk_body_crlf(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_FAIL );
        REQUIRE( called );
    }

    SECTION("resp body skip trailers retry.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_TRAILERS;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_resp_body_skip_trailers(&http);
        REQUIRE( http._state == KHC_STATE_RESP_BODY_SKIP_TRAILERS );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("resp body skip trailers failed.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_TRAILERS;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_resp_body_skip_trailers(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("close retry.") {
        http._state = KHC_STATE_CLOSE;

        called = false;
        s_ctx.on_close = [=, &called](void* socket_context) {
            called = true;
            return KHC_SOCK_AGAIN;
        };

        khc_state_close(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("close failed.") {
        http._state = KHC_STATE_CLOSE;

        called = false;
        s_ctx.on_close = [=, &called](void* socket_context) {
            called = true;
            return KHC_SOCK_FAIL;
        };

        khc_state_close(&http);
        REQUIRE( http._state == KHC_STATE_FINISHED );
        REQUIRE( http._result == KHC_ERR_SOCK_CLOSE );
        REQUIRE( called );
    }
}

TEST_CASE( "Enable insecure" ) {
    khc http;
    khc_init(&http);

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);

    REQUIRE( http._enable_insecure == 0);
    khc_enable_insecure(&http, 1);
    REQUIRE( http._enable_insecure == 1);

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, unsigned int port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 80 );
        return KHC_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        const char req_line[] = "GET http://api.kii.com/api/apps HTTP/1.1\r\n";
        REQUIRE( length == strlen(req_line) );
        REQUIRE( strncmp(buffer, req_line, length) == 0 );
        *out_sent_length = length;
        return KHC_SOCK_OK;
    };

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}
