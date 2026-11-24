#include <string.h>
#include "catch_amalgamated.hpp"
#include <khc.h>
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <sstream>
#include <vector>

TEST_CASE( "HTTP minimal" ) {
    khc_t http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    char stream_buff[buff_size];
    char resp_header_buff[resp_header_buff_size];
    khc_set_stream_buff(&http, stream_buff, buff_size);
    khc_set_resp_header_buff(&http, resp_header_buff, resp_header_buff_size);

    khct::http::Resp resp;
    resp.headers = { "HTTP/1.0 200 OK" };

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL, 0);

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());

    khct::cb::IOCtx io_ctx;
    // no set cb_read.
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return EBISU_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    // The head goes in the stream buffer, and out in one send.
    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        REQUIRE( std::string(buffer, length) == "GET /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\nConnection: Close\r\n\r\n" );
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    khc_state_req_body_send(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == false );
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
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_FRAGMENT );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_fragment(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_recv = [=, &called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == buff_size);
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };
    khc_state_resp_body_read(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == true );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return EBISU_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}

TEST_CASE( "HTTP 1.1 chunked minimal" ) {
    khc_t http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    char stream_buff[buff_size];
    char resp_header_buff[resp_header_buff_size];
    khc_set_stream_buff(&http, stream_buff, buff_size);
    khc_set_resp_header_buff(&http, resp_header_buff, resp_header_buff_size);

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
    khc_set_req_headers(&http, NULL, 0);

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return EBISU_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    // A GET has no body, so none follows the headers, not even the chunk
    // that would end an empty one, though a read callback is set.
    // The head goes in the stream buffer, and out in one send.
    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        called = true;
        REQUIRE( std::string(buffer, length) == "GET /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\nConnection: Close\r\n\r\n" );
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    khc_state_req_body_send(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == false );
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
    REQUIRE( http._chunked_resp == false );
    REQUIRE( http._read_end == false );
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
    REQUIRE( http._chunked_resp == false );
    REQUIRE( http._read_end == false );
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
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 0 );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_parse_chunk_size(&http);
    REQUIRE( http._state == KHC_STATE_READ_CHUNK_SIZE_FROM_HEADER_BUFF );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 0 );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_read_chunk_size_from_header_buff(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_PARSE_CHUNK_SIZE );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 5 );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_parse_chunk_size(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_SKIP_TRAILERS );
    REQUIRE( http._chunk_size == 0 );
    REQUIRE( http._body_read_size == 2 );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    // The empty line after the last chunk has come: nothing is left to read.
    called = false;
    s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        *out_actual_length = 0;
        return EBISU_SOCK_OK;
    };
    khc_state_resp_body_skip_trailers(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( !called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return EBISU_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}

TEST_CASE( "Socket send partial" ) {
    khc_t http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;

    char stream_buff[buff_size];
    char resp_header_buff[resp_header_buff_size];
    khc_set_stream_buff(&http, stream_buff, buff_size);
    khc_set_resp_header_buff(&http, resp_header_buff, resp_header_buff_size);

    khct::http::Resp resp;
    resp.headers = { "HTTP/1.0 200 OK" };

    const char req_headers[] = "X-KII-dummy-Header: test partial.\r\n";

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "POST");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, req_headers, strlen(req_headers));

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());

    khct::cb::IOCtx io_ctx;
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 443 );
        return EBISU_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    // The head goes in the stream buffer, and the body after it, until the
    // body ends: the chunk of size 0 that ends it fits too.
    const std::string head =
        "POST /api/apps HTTP/1.1\r\n"
        "HOST: api.kii.com\r\n"
        "X-KII-dummy-Header: test partial.\r\n"
        "Transfer-Encoding: chunked\r\n"
        "Connection: Close\r\n\r\n";
    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._req_frame_size == head.size() );

    // Each read is given what is left of the buffer, less room for the size
    // line of the longest chunk that fits, of 3 hex digits here, with its
    // CRLF, and for the CRLF after the data.
    const size_t room = buff_size - head.size();
    REQUIRE( 0x100 <= room );
    REQUIRE( room <= 0xfff );
    std::vector<size_t> asked;
    io_ctx.on_read = [=, &asked](char *buffer, size_t size, void *userdata) {
        asked.push_back(size);
        if (asked.size() > 1) {
            return (size_t)0;
        }
        const char body[] = "http body";
        memcpy(buffer, body, strlen(body));
        return strlen(body);
    };
    khc_state_req_body_read(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( http._read_req_end == true );
    REQUIRE( asked == std::vector<size_t>{ room - 5 - 2,
            room - strlen("9\r\nhttp body\r\n") - 5 - 2 } );

    // Of what a send doesn't take, the next gets the rest.
    const std::string request = head + "9\r\nhttp body\r\n" + "0\r\n\r\n";
    std::string sent;
    std::vector<size_t> offered;
    s_ctx.on_send = [=, &sent, &offered](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        offered.push_back(length);
        *out_sent_length = length < 8 ? length : 8;
        sent.append(buffer, *out_sent_length);
        return EBISU_SOCK_OK;
    };
    while (http._state == KHC_STATE_REQ_BODY_SEND) {
        khc_state_req_body_send(&http);
        REQUIRE( http._result == KHC_ERR_OK );
    }
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
    REQUIRE( sent == request );
    REQUIRE( offered.size() == (request.size() + 7) / 8 );
    REQUIRE( offered[0] == request.size() );
    REQUIRE( offered[1] == request.size() - 8 );

    called = false;
    auto is = resp.to_istringstream();
    s_ctx.on_recv = [=, &called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == resp_header_buff_size - 1 );
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };

    khc_state_resp_status_read(&http);
    REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
    REQUIRE( http._read_end == false );
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
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_resp_header_callback(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_FRAGMENT );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    khc_state_resp_body_fragment(&http);
    REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
    REQUIRE( http._read_end == false );
    REQUIRE( http._result == KHC_ERR_OK );

    called = false;
    s_ctx.on_recv = [=, &called, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        called = true;
        REQUIRE( length_to_read == buff_size);
        *out_actual_length = is.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };
    khc_state_resp_body_read(&http);
    REQUIRE( http._state == KHC_STATE_CLOSE );
    REQUIRE( http._read_end == true );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    called = false;
    s_ctx.on_close = [=, &called](void* socket_ctx) {
        called = true;
        return EBISU_SOCK_OK;
    };
    khc_state_close(&http);
    REQUIRE( http._state == KHC_STATE_FINISHED );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );
}

TEST_CASE( "state abnormal tests." ) {
    khc_t http;
    khc_init(&http);
    const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
    const size_t resp_header_buff_size = DEFAULT_RESP_HEADER_BUFF_SIZE;
    char stream_buff[buff_size];
    char resp_header_buff[resp_header_buff_size];

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL, 0);
    khc_set_stream_buff(&http, stream_buff, buff_size);
    khc_set_resp_header_buff(&http, resp_header_buff, resp_header_buff_size);

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());

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
        s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
            called = true;
            return EBISU_SOCK_AGAIN;
        };

        khc_state_connect(&http);
        REQUIRE( http._state == KHC_STATE_CONNECT );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state connect failed.") {
        called = false;
        s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
            called = true;
            return EBISU_SOCK_FAIL;
        };

        khc_state_connect(&http);
        REQUIRE( http._state == KHC_STATE_FINISHED );
        REQUIRE( http._result == KHC_ERR_SOCK_CONNECT );
        REQUIRE( called );
    }

    SECTION("state req line puts the head in the buffer, and sends nothing.") {
        http._state = KHC_STATE_REQ_LINE;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return EBISU_SOCK_FAIL;
        };

        khc_state_req_line(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( http._req_frame_size ==
                strlen("GET /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\nConnection: Close\r\n\r\n") );
        REQUIRE_FALSE( called );
    }

    SECTION("state req head send retry.") {
        http._state = KHC_STATE_REQ_LINE;
        khc_state_req_line(&http);

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( http._again );
        REQUIRE( called );
    }

    SECTION("state req head send failed.") {
        http._state = KHC_STATE_REQ_LINE;
        khc_state_req_line(&http);

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return EBISU_SOCK_FAIL;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("a head larger than the stream buffer goes a buffer at a time.") {
        const size_t small = 16;
        khc_set_stream_buff(&http, stream_buff, small);
        // Headers across several of them.
        const std::string headers = "X-A: 1\r\nX-Long: " + std::string(40, 'x') + "\r\n";
        khc_set_req_headers(&http, headers.data(), headers.size());
        http._state = KHC_STATE_REQ_LINE;

        std::string sent;
        std::vector<size_t> offered;
        s_ctx.on_send = [=, &sent, &offered](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            offered.push_back(length);
            sent.append(buffer, length);
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };

        while (http._state == KHC_STATE_REQ_LINE || http._state == KHC_STATE_REQ_BODY_SEND) {
            state_handlers[http._state](&http);
            REQUIRE( http._result == KHC_ERR_OK );
        }
        const std::string head = "GET /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\n" + headers +
            "Connection: Close\r\n\r\n";
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
        REQUIRE( sent == head );
        REQUIRE( offered.size() == (head.size() + small - 1) / small );
        for (size_t i = 0; i + 1 < offered.size(); ++i) {
            REQUIRE( offered[i] == small );
        }
    }

    SECTION("state req body send retry.") {
        http._state = KHC_STATE_REQ_BODY_SEND;
        http._req_frame_start = 0;
        http._req_frame_size = 16;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );
    }

    SECTION("state req body send failed.") {
        http._state = KHC_STATE_REQ_BODY_SEND;
        http._req_frame_start = 0;
        http._req_frame_size = 16;
        http._sent_length = 0;

        called = false;
        s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
            called = true;
            return EBISU_SOCK_FAIL;
        };

        khc_state_req_body_send(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_SEND );
        REQUIRE( called );
    }

    SECTION("resp status read no crlf, with room for more.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            strcpy(buffer, "no crlf");
            *out_actual_length = strlen(buffer);
            return EBISU_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_READ );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( http._resp_header_read_size == strlen("no crlf") );
        REQUIRE( called );
    }

    SECTION("resp status read no crlf, with the buffer full.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            memset(buffer, 'a', length_to_read);
            *out_actual_length = length_to_read;
            return EBISU_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
        REQUIRE( called );
    }

    SECTION("resp status read closed.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            *out_actual_length = 0;
            return EBISU_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp status read retry.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );

        khc_state_resp_status_parse(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_INVALID_RESPONSE );
    }

    SECTION("resp status parse non-ASCII byte.") {
        http._state = KHC_STATE_RESP_STATUS_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            strcpy(buffer, "HTTP/1.1 2\xb9" "0 OK\r\n");
            *out_actual_length = strlen(buffer);
            return EBISU_SOCK_OK;
        };

        khc_state_resp_status_read(&http);
        REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( called );

        khc_state_resp_status_parse(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_INVALID_RESPONSE );
    }

    SECTION("resp header read retry.") {
        http._state = KHC_STATE_RESP_HEADER_READ;
        http._resp_header_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
    }

    SECTION("resp header skip failed by Transfer-Encoding.") {
        http._state = KHC_STATE_RESP_HEADER_SKIP;
        char sbuff[27];
        strcpy(sbuff, "Transfer-Encoding: chunked");
        khc_set_resp_header_buff(&http, sbuff, 26);
        http._resp_header_read_size = 26;

        khc_state_resp_header_skip(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_TOO_LARGE_DATA );
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
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
        };

        khc_state_resp_body_read_chunk_size(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body read chunk size closed.") {
        http._state = KHC_STATE_RESP_BODY_READ_CHUNK_SIZE;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            *out_actual_length = 0;
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_OK;
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
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_OK;
        };

        khc_state_resp_body_read_chunk_body(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body skip chunk body crlf retry.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_CHUNK_BODY_CRLF;
        http._body_read_size = 0;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_OK;
        };

        khc_state_resp_body_skip_chunk_body_crlf(&http);
        REQUIRE( http._state == KHC_STATE_CLOSE );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( called );
    }

    SECTION("resp body skip trailers retry.") {
        http._state = KHC_STATE_RESP_BODY_SKIP_TRAILERS;

        called = false;
        s_ctx.on_recv = [=, &called](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            called = true;
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
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
            return EBISU_SOCK_AGAIN;
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
            return EBISU_SOCK_FAIL;
        };

        khc_state_close(&http);
        REQUIRE( http._state == KHC_STATE_FINISHED );
        REQUIRE( http._result == KHC_ERR_OK );
        REQUIRE( khc_get_close_failed(&http) );
        REQUIRE( called );
    }

    SECTION("close failed after the request did.") {
        http._state = KHC_STATE_CLOSE;
        http._result = KHC_ERR_SOCK_RECV;

        called = false;
        s_ctx.on_close = [=, &called](void* socket_context) {
            called = true;
            return EBISU_SOCK_FAIL;
        };

        khc_state_close(&http);
        REQUIRE( http._state == KHC_STATE_FINISHED );
        REQUIRE( http._result == KHC_ERR_SOCK_RECV );
        REQUIRE( khc_get_close_failed(&http) );
        REQUIRE( called );
    }
}

TEST_CASE( "Enable insecure" ) {
    khc_t http;
    khc_init(&http);
    char stream_buff[DEFAULT_STREAM_BUFF_SIZE];
    khc_set_stream_buff(&http, stream_buff, sizeof(stream_buff));

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL, 0);

    REQUIRE( http._enable_insecure == false );
    khc_enable_insecure(&http, true);
    REQUIRE( http._enable_insecure == true );

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());

    khc_state_idle(&http);
    REQUIRE( http._state == KHC_STATE_CONNECT );
    REQUIRE( http._result == KHC_ERR_OK );

    bool called = false;
    s_ctx.on_connect = [=, &called](void* socket_context, const char* host, uint16_t port) {
        called = true;
        REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
        REQUIRE( strlen(host) == strlen("api.kii.com") );
        REQUIRE( port == 80 );
        return EBISU_SOCK_OK;
    };

    khc_state_connect(&http);
    REQUIRE( http._state == KHC_STATE_REQ_LINE );
    REQUIRE( http._result == KHC_ERR_OK );
    REQUIRE( called );

    khc_state_req_line(&http);
    REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
    REQUIRE( http._result == KHC_ERR_OK );
    // In the buffer the head is put in: insecure or not, the request line
    // carries the path alone.
    REQUIRE( std::string(http._stream_buff, strlen("GET /api/apps HTTP/1.1\r\n")) ==
            "GET /api/apps HTTP/1.1\r\n" );
}

// What the request line state puts first in the stream buffer: the request
// line.
static std::string sent_request_line(const char* path, bool absolute, bool insecure) {
    khc_t http;
    khc_init(&http);
    char stream_buff[DEFAULT_STREAM_BUFF_SIZE];
    khc_set_stream_buff(&http, stream_buff, sizeof(stream_buff));
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "GET");
    khc_set_path(&http, path);
    khc_enable_absolute_target(&http, absolute);
    khc_enable_insecure(&http, insecure);

    khc_state_req_line(&http);
    std::string head(stream_buff, http._req_frame_size);
    return head.substr(0, head.find("\r\n") + 2);
}

TEST_CASE( "The request line carries the path, for a server" ) {
    CHECK( sent_request_line("/api/apps", false, false) == "GET /api/apps HTTP/1.1\r\n" );
    CHECK( sent_request_line("/api/apps", false, true) == "GET /api/apps HTTP/1.1\r\n" );
}

// Which an origin server has to be sent for an empty one, as a request line has
// to have something there.
TEST_CASE( "An empty path goes out as /" ) {
    CHECK( sent_request_line("", false, false) == "GET / HTTP/1.1\r\n" );
    CHECK( sent_request_line("", true, false) == "GET https://api.kii.com/ HTTP/1.1\r\n" );
}

TEST_CASE( "The request line carries the whole URL, for a forward proxy" ) {
    CHECK( sent_request_line("/api/apps", true, false) == "GET https://api.kii.com/api/apps HTTP/1.1\r\n" );
    CHECK( sent_request_line("/api/apps", true, true) == "GET http://api.kii.com/api/apps HTTP/1.1\r\n" );
}

// What khc_perform() sends for a request with the method given and a read
// callback giving body.
static std::string sent_request(const char* method, const char* body) {
    khc_t http;
    khc_init(&http);
    char stream_buff[DEFAULT_STREAM_BUFF_SIZE];
    char resp_header_buff[DEFAULT_RESP_HEADER_BUFF_SIZE];
    khc_set_stream_buff(&http, stream_buff, sizeof(stream_buff));
    khc_set_resp_header_buff(&http, resp_header_buff, sizeof(resp_header_buff));
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, method);
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL, 0);

    std::string sent;
    std::istringstream resp("HTTP/1.1 204 No Content\r\n\r\n");
    khct::cb::SockCtx s_ctx;
    s_ctx.on_connect = [](void* socket_context, const char* host, uint16_t port) {
        return EBISU_SOCK_OK;
    };
    s_ctx.on_send = [&sent](void* socket_context, const char* buffer, size_t length, size_t* out_sent_length) {
        sent.append(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    s_ctx.on_recv = [&resp](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = resp.read(buffer, length_to_read).gcount();
        return EBISU_SOCK_OK;
    };
    s_ctx.on_close = [](void* socket_context) {
        return EBISU_SOCK_OK;
    };
    khc_set_socket(&http, s_ctx.socket());

    bool body_read = false;
    khct::cb::IOCtx io_ctx;
    io_ctx.on_read = [&body_read, body](char *buffer, size_t size, void *userdata) {
        size_t len = body_read ? 0 : strlen(body);
        memcpy(buffer, body, len);
        body_read = true;
        return len;
    };
    io_ctx.on_header = [](char *buffer, size_t size, void *userdata) {
        return size;
    };
    io_ctx.on_write = [](char *buffer, size_t size, void *userdata) {
        return size;
    };
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);

    REQUIRE( khc_perform(&http) == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 204 );
    return sent;
}

TEST_CASE( "Only a method that can have a body sends one" ) {
    const std::string target = " /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\n";
    // Nothing after the headers, which announce no body: not even the chunk
    // that would end an empty one.
    for (const char* method : {"GET", "HEAD", "DELETE"}) {
        INFO( method );
        REQUIRE( sent_request(method, "body") == method + target + "Connection: Close\r\n\r\n" );
    }
    REQUIRE( sent_request("POST", "body") == "POST" + target +
            "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n4\r\nbody\r\n0\r\n\r\n" );
}
