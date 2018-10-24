#define CATCH_CONFIG_MAIN

#include <string.h>
#include "catch.hpp"
#include <khc.h>
#include "khc_impl.h"
#include "khc_state_impl.h"
#include "http_test.h"
#include "test_callbacks.h"
#include <sstream>

TEST_CASE( "HTTP minimal" ) {
  khc http;
  khc_set_zero(&http);
  const size_t buff_size = DEFAULT_STREAM_BUFF_SIZE;
  const size_t resp_header_buff_size = RESP_HEADER_BUFF_SIZE;

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
  REQUIRE( http._state == KHC_STATE_RESP_HEADERS_ALLOC );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

  khc_state_resp_headers_alloc(&http);
  REQUIRE( http._state == KHC_STATE_RESP_HEADERS_READ );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( *http._resp_header_buffer == '\0' );
  REQUIRE( http._resp_header_buffer == http._resp_header_buffer_current_pos );
  REQUIRE (http._resp_header_buffer_size == resp_header_buff_size );

  called = false;
  auto is = resp.to_istringstream();
  s_ctx.on_recv = [=, &called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
    called = true;
    REQUIRE( length_to_read == resp_header_buff_size - 1 );
    *out_actual_length = is.read(buffer, length_to_read).gcount();
    return KHC_SOCK_OK;
  };

  khc_state_resp_headers_read(&http);
  REQUIRE( http._state == KHC_STATE_RESP_STATUS_PARSE );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );
  char buffer[resp_header_buff_size];
  size_t len = resp.to_istringstream().read((char*)&buffer, resp_header_buff_size - 1).gcount();
  REQUIRE( http._resp_header_read_size == len );
  REQUIRE( called );

  khc_state_resp_status_parse(&http);
  REQUIRE( khc_get_status_code(&http) == 200 );
  REQUIRE( http._state == KHC_STATE_RESP_HEADERS_CALLBACK );

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

  khc_state_resp_headers_callback(&http);
  REQUIRE( http._state == KHC_STATE_RESP_BODY_READ );
  REQUIRE( http._read_end == 0 );
  REQUIRE( http._result == KHC_ERR_OK );
  REQUIRE( called );

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

TEST_CASE( "_contains_chunked tests" ) {
  SECTION( "chunked exists" ) {
    const char str[] = "chunked\r\n";
    int res = _contains_chunked(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "ignore case" ) {
    const char str[] = "ChUnkEd\r\n";
    int res = _contains_chunked(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "chunked and others exist" ) {
    const char str[] = "gzip, compress, chunked, deflate\r\n";
    int res = _contains_chunked(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "chunked no exist" ) {
    const char str[] = "gzip, compress, deflate\r\n";
    int res = _contains_chunked(str, strlen(str) - 2);
    REQUIRE( res == 0);
  }
  SECTION( "empty string" ) {
    int res = _contains_chunked("", 0);
    REQUIRE( res == 0);
  }
  SECTION( "NULL" ) {
    int res = _contains_chunked(NULL, 0);
    REQUIRE( res == 0);
  }
}

TEST_CASE( "_is_chunked_encoding tests" ) {
  SECTION( "default pattern header" ) {
    const char str[] = "transfer-encoding:chunked\r\n";
    int res = _is_chunked_encoding(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "optional white spaces exist" ) {
    const char str[] = "transfer-encoding: \t chunked\r\n";
    int res = _is_chunked_encoding(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "ignore case" ) {
    const char str[] = "TrAnSfeR-enCodING: chunked\r\n";
    int res = _is_chunked_encoding(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "chunked and others exist" ) {
    const char str[] = "Transfer-Encoding: gzip, chunked, compress\r\n";
    int res = _is_chunked_encoding(str, strlen(str) - 2);
    REQUIRE( res == 1);
  }
  SECTION( "other header" ) {
    const char str[] = "Content-Length: 0\r\n";
    int res = _is_chunked_encoding(str, strlen(str) - 2);
    REQUIRE( res == 0);
  }
  SECTION( "empty string" ) {
    int res = _is_chunked_encoding("", 0);
    REQUIRE( res == 0);
  }
  SECTION( "NULL" ) {
    int res = _is_chunked_encoding(NULL, 0);
    REQUIRE( res == 0);
  }
}

TEST_CASE( "_extract_content_length tests" ) {
  SECTION( "default pattern header exist" ) {
    size_t len = 0;
    const char str[] = "content-length:123\r\n";
    int res = _extract_content_length(str, strlen(str) - 2, &len);
    REQUIRE( res == 1);
    REQUIRE( len == 123);
  }
  SECTION( "optional white spaces exist" ) {
    size_t len = 0;
    const char str[] = "content-length:\t \t 45678\r\n";
    int res = _extract_content_length(str, strlen(str) - 2, &len);
    REQUIRE( res == 1);
    REQUIRE( len == 45678);
  }
  SECTION( "ignore case" ) {
    size_t len = 0;
    const char str[] = "cOnTeNt-LenGtH: 1234\r\n";
    int res = _extract_content_length(str, strlen(str) - 2, &len);
    REQUIRE( res == 1);
    REQUIRE( len == 1234);
  }
  SECTION( "other header" ) {
    size_t len = 0;
    const char str[] = "Transfer-Encoding: Chunked\r\n";
    int res = _extract_content_length(str, strlen(str) - 2, &len);
    REQUIRE( res == 0);
    REQUIRE( len == 0);
  }
  SECTION( "empty string" ) {
    size_t len = 0;
    int res = _extract_content_length("", 0, &len);
    REQUIRE( res == 0);
    REQUIRE( len == 0);
  }
  SECTION( "NULL" ) {
    size_t len = 0;
    int res = _extract_content_length(NULL, 0, &len);
    REQUIRE( res == 0);
    REQUIRE( len == 0);
  }
}
