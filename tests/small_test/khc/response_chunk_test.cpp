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
  khc_set_zero(&http);
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
  khc_set_cb_sock_connect(&http, khct::cb::mock_connect, 15, 15, &s_ctx);
  khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
  khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
  khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

  khct::cb::IOCtx io_ctx;
  khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
  khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
  khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

  int on_connect_called = 0;
  s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port, unsigned int to_recv_in_seconds, unsigned int to_send_in_seconds) {
    ++on_connect_called;
    REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
    REQUIRE( strlen(host) == strlen("api.kii.com") );
    REQUIRE( port == 443 );
    REQUIRE( to_recv_in_seconds == 15 );
    REQUIRE( to_send_in_seconds == 15 );
    return KHC_SOCK_OK;
  };

  int on_send_called = 0;
  s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length) {
    ++on_send_called;
    return KHC_SOCK_OK;
  };

  int on_read_called = 0;
  io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_read_called;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
    return 0;
  };

  int on_recv_called = 0;
  auto is = resp.to_istringstream();
  s_ctx.on_recv = [=, &on_recv_called, &resp, &is](void* socket_context, char* buffer, size_t length_to_read, size_t* out_actual_length) {
    ++on_recv_called;
    if (on_recv_called == 1)
      REQUIRE( length_to_read == 1023 );
    if (on_recv_called == 2)
      REQUIRE( length_to_read == 1024 );
    *out_actual_length = is.read(buffer, length_to_read).gcount();
    return KHC_SOCK_OK;
  };

  int on_header_called = 0;
  io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    const char* header = resp.headers[on_header_called].c_str();
    size_t len = strlen(header);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, header, len) == 0 );
    ++on_header_called;
    return size * count;
  };

  const char* chunkedBody[] = {
    "{",
    "  \"id\" : \"b56270b00022-171b-7e11-b35e-0911a10d\",",
    "  \"access_token\" : \"cHltZmFtc3cxMnJn._I4i4fNNTjRfWia9juCpRgipPUNWD1-6bobnfJvQPms\",",
    "  \"expires_in\" : 2147483646,",
    "  \"token_type\" : \"Bearer\"",
    "}"
  };
  int on_write_called = 0;
  io_ctx.on_write = [=, &chunkedBody, &on_write_called](char *buffer, size_t size, size_t count, void *userdata) {
    REQUIRE ( on_write_called < 6 );
    REQUIRE ( strlen(chunkedBody[on_write_called]) == size * count );
    REQUIRE ( strncmp(buffer, chunkedBody[on_write_called], size * count) == 0 );
    ++on_write_called;
    return size * count;
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
  REQUIRE( on_recv_called == 2 );
  REQUIRE( on_header_called == 12 );
  REQUIRE( on_write_called == 6 );
  REQUIRE( on_close_called == 1 );
}

TEST_CASE( "small buffer size test" ) {
  khc http;
  khc_set_zero(&http);
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
  khc_set_cb_sock_connect(&http, khct::cb::mock_connect, 15, 15, &s_ctx);
  khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
  khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
  khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

  khct::cb::IOCtx io_ctx;
  khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
  khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
  khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

  int on_connect_called = 0;
  s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port, unsigned int to_recv_in_seconds, unsigned int to_send_in_seconds) {
    ++on_connect_called;
    REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
    REQUIRE( strlen(host) == strlen("api.kii.com") );
    REQUIRE( port == 443 );
    REQUIRE( to_recv_in_seconds == 15 );
    REQUIRE( to_send_in_seconds == 15 );
    return KHC_SOCK_OK;
  };

  int on_send_called = 0;
  s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length) {
    ++on_send_called;
    return KHC_SOCK_OK;
  };

  int on_read_called = 0;
  io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_read_called;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
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
  io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    const char* header = resp.headers[on_header_called].c_str();
    size_t len = strlen(header);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, header, len) == 0 );
    ++on_header_called;
    return size * count;
  };

  ostringstream oss;
  int on_write_called = 0;
  io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_write_called;
    oss.write(buffer, size * count);
    return size * count;
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
  khc_set_zero(&http);
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
  khc_set_cb_sock_connect(&http, khct::cb::mock_connect, 15, 15, &s_ctx);
  khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
  khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
  khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

  khct::cb::IOCtx io_ctx;
  khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
  khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
  khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

  int on_connect_called = 0;
  s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port, unsigned int to_recv_in_seconds, unsigned int to_send_in_seconds) {
    ++on_connect_called;
    REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
    REQUIRE( strlen(host) == strlen("api.kii.com") );
    REQUIRE( port == 443 );
    REQUIRE( to_recv_in_seconds == 15 );
    REQUIRE( to_send_in_seconds == 15 );
    return KHC_SOCK_OK;
  };

  int on_send_called = 0;
  s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length) {
    ++on_send_called;
    return KHC_SOCK_OK;
  };

  int on_read_called = 0;
  io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_read_called;
    REQUIRE( size == 1);
    REQUIRE( count == buff_size);
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
  io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    const char* header = resp.headers[on_header_called].c_str();
    size_t len = strlen(header);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, header, len) == 0 );
    ++on_header_called;
    return size * count;
  };

  ostringstream oss;
  int on_write_called = 0;
  io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_write_called;
    oss.write(buffer, size * count);
    return size * count;
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
  khc_set_zero(&http);

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
  khc_set_cb_sock_connect(&http, khct::cb::mock_connect, 15, 15, &s_ctx);
  khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
  khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
  khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

  khct::cb::IOCtx io_ctx;
  khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);
  khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
  khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

  int on_connect_called = 0;
  s_ctx.on_connect = [=, &on_connect_called](void* socket_context, const char* host, unsigned int port, unsigned int to_recv_in_seconds, unsigned int to_send_in_seconds) {
    ++on_connect_called;
    REQUIRE( strncmp(host, "api.kii.com", strlen("api.kii.com")) == 0 );
    REQUIRE( strlen(host) == strlen("api.kii.com") );
    REQUIRE( port == 443 );
    REQUIRE( to_recv_in_seconds == 15 );
    REQUIRE( to_send_in_seconds == 15 );
    return KHC_SOCK_OK;
  };

  int on_send_called = 0;
  s_ctx.on_send = [=, &on_send_called](void* socket_context, const char* buffer, size_t length) {
    ++on_send_called;
    return KHC_SOCK_OK;
  };

  int on_read_called = 0;
  io_ctx.on_read = [=, &on_read_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_read_called;
    REQUIRE( size == 1);
    REQUIRE( count == DEFAULT_STREAM_BUFF_SIZE);
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
  io_ctx.on_header = [=, &on_header_called, &resp](char *buffer, size_t size, size_t count, void *userdata) {
    const char* header = resp.headers[on_header_called].c_str();
    size_t len = strlen(header);
    REQUIRE( size == 1);
    REQUIRE( count == len );
    REQUIRE( strncmp(buffer, header, len) == 0 );
    ++on_header_called;
    return size * count;
  };

  ostringstream oss;
  int on_write_called = 0;
  io_ctx.on_write = [=, &oss, &on_write_called](char *buffer, size_t size, size_t count, void *userdata) {
    ++on_write_called;
    oss.write(buffer, size * count);
    return size * count;
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