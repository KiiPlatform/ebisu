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
  khc_set_zero(&http);
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char req_line[] = "GET https://api.kii.com/api/apps HTTP/1.1\r\n";
    REQUIRE( length == strlen(req_line) );
    REQUIRE( strncmp(buffer, req_line, length) == 0 );
    return KHC_SOCK_OK;
  };

  khc_state_req_line(&http);
  REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char host_hdr[] = "HOST: api.kii.com\r\n";
    REQUIRE( length == strlen(host_hdr) );
    REQUIRE( strncmp(buffer, host_hdr, length) == 0 );
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    REQUIRE( length == 49 );
    REQUIRE( strncmp(buffer, "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n", 49) == 0 );
    return KHC_SOCK_OK;
  };

  khc_state_req_header_end(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_read = [=, &called](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "9\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_size(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "http body";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_crlf(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_read = [=, &called](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
    return 0;
  };
  khc_state_req_body_read(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( http._read_req_end == 1 );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "0\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_size(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
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
  io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    const char* status_line = resp.headers[0].c_str();
    size_t len = strlen(status_line);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, status_line, len) == 0 );
    return size * count;
  };

  khc_state_resp_header_callback(&http);
  REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  khc_state_resp_header_callback(&http);
  REQUIRE( http._state == KHC_STATE_RESP_BODY_FLAGMENT );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );

  khc_state_resp_body_flagment(&http);
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
  khc_set_zero(&http);
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char req_line[] = "GET https://api.kii.com/api/apps HTTP/1.1\r\n";
    REQUIRE( length == strlen(req_line) );
    REQUIRE( strncmp(buffer, req_line, length) == 0 );
    return KHC_SOCK_OK;
  };

  khc_state_req_line(&http);
  REQUIRE( http._state == KHC_STATE_REQ_HOST_HEADER );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char host_hdr[] = "HOST: api.kii.com\r\n";
    REQUIRE( length == strlen(host_hdr) );
    REQUIRE( strncmp(buffer, host_hdr, length) == 0 );
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    REQUIRE( length == 49 );
    REQUIRE( strncmp(buffer, "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n", 49) == 0 );
    return KHC_SOCK_OK;
  };

  khc_state_req_header_end(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_read = [=, &called](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
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
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "9\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_size(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "http body";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_crlf(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_READ );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_read = [=, &called](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
    return 0;
  };
  khc_state_req_body_read(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_SIZE );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( http._read_req_end == 1 );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "0\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
    return KHC_SOCK_OK;
  };
  khc_state_req_body_send_size(&http);
  REQUIRE( http._state == KHC_STATE_REQ_BODY_SEND_CRLF );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  s_ctx.on_send = [=, &called](void* socket_context, const char* buffer, size_t length) {
    called = true;
    const char body[] = "\r\n";
    REQUIRE( length == strlen(body) );
    REQUIRE( strncmp(buffer, body, length) == 0 );
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
  io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    const char* status_line = resp.headers[0].c_str();
    size_t len = strlen(status_line);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, status_line, len) == 0 );
    return size * count;
  };

  khc_state_resp_header_callback(&http);
  REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
  REQUIRE( http._chunked_resp == 0 );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    const char* host_line = resp.headers[1].c_str();
    size_t len = strlen(host_line);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, host_line, len) == 0 );
    return size * count;
  };

  khc_state_resp_header_callback(&http);
  REQUIRE( http._state == KHC_STATE_RESP_HEADER_CALLBACK );
  REQUIRE( http._chunked_resp == 0 );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  called = false;
  io_ctx.on_header = [=, &called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    called = true;
    const char* chunked_line = resp.headers[2].c_str();
    size_t len = strlen(chunked_line);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, chunked_line, len) == 0 );
    return size * count;
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
