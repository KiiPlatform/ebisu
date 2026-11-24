#include "catch_amalgamated.hpp"
#include <khc.h>
#include "test_callbacks.h"
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Request {
    std::string method = "POST";
    std::string body;
    size_t stream_buff_size = 1024;
    // Set with khc_set_req_body_length() unless KHC_REQ_BODY_LENGTH_UNKNOWN.
    size_t body_length = KHC_REQ_BODY_LENGTH_UNKNOWN;
    // Whether the read callback says it read one byte more than it was asked
    // for.
    bool overrun = false;
    // The most the read callback gives at a time, 0 for as much as asked.
    size_t piece = 0;
};

struct Sent {
    khc_code_t res;
    // Up to the blank line that ends them.
    std::string headers;
    // What of each send comes after the headers: the first may be the rest
    // of the send that ended them.
    std::vector<std::string> body_sends;
    // Each send, whole.
    std::vector<std::string> sends;
    // What the read callback was asked for, each time.
    std::vector<size_t> read_sizes;
    int closes;
};

// Performs req, answered with 204, and records what khc sent.
Sent perform(const Request& req) {
    std::vector<char> stream_buff(req.stream_buff_size);
    khc_t http;
    khc_init(&http);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, req.method.c_str());
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_stream_buff(&http, stream_buff.data(), stream_buff.size());
    if (req.body_length != KHC_REQ_BODY_LENGTH_UNKNOWN) {
        khc_set_req_body_length(&http, req.body_length);
    }

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());
    khct::cb::IOCtx io_ctx;
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);
    khc_set_cb_read(&http, khct::cb::cb_read, &io_ctx);

    Sent sent = {};
    std::vector<std::string> sends;
    s_ctx.on_connect = [](void*, const char*, uint16_t) {
        return EBISU_SOCK_OK;
    };
    s_ctx.on_send = [&sends](void*, const char* buffer, size_t length, size_t* out_sent_length) {
        sends.emplace_back(buffer, length);
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    const std::string response = "HTTP/1.1 204 No Content\r\n\r\n";
    size_t offset = 0;
    s_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = response.copy(buffer, length_to_read, offset);
        offset += *out_actual_length;
        return EBISU_SOCK_OK;
    };
    s_ctx.on_close = [&sent](void*) {
        ++sent.closes;
        return EBISU_SOCK_OK;
    };
    io_ctx.on_header = [](char*, size_t size, void*) {
        return size;
    };
    io_ctx.on_write = [](char*, size_t size, void*) {
        return size;
    };
    size_t read_offset = 0;
    io_ctx.on_read = [&](char* buffer, size_t size, void*) {
        sent.read_sizes.push_back(size);
        size_t n = req.body.copy(buffer, req.piece > 0 && req.piece < size ? req.piece : size, read_offset);
        read_offset += n;
        return req.overrun && n > 0 ? size + 1 : n;
    };

    sent.res = khc_perform(&http);

    sent.sends = sends;
    std::string wire;
    for (const std::string& s : sends) {
        wire += s;
    }
    size_t head_end = wire.find("\r\n\r\n");
    head_end = (head_end == std::string::npos) ? wire.size() : head_end + 4;
    sent.headers = wire.substr(0, head_end);
    size_t pos = 0;
    for (const std::string& s : sends) {
        if (pos + s.size() > head_end) {
            size_t from = (pos < head_end) ? head_end - pos : 0;
            sent.body_sends.push_back(s.substr(from));
        }
        pos += s.size();
    }
    return sent;
}

std::string body_of(size_t size) {
    std::string body;
    for (size_t i = 0; i < size; ++i) {
        body += (char)('a' + i % 26);
    }
    return body;
}

std::string chunk(const std::string& size_line, const std::string& data) {
    return size_line + "\r\n" + data + "\r\n";
}

bool has(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

}

// The size line, the data and the CRLF of each chunk went out in three sends:
// over TLS, three records for each chunk. The head goes in the stream buffer
// first, and the body after it, until the buffer is full or the body ends.
TEST_CASE( "Each chunk of a request body is sent in one write" ) {
    const std::string head = "POST /api/apps HTTP/1.1\r\nHOST: api.kii.com\r\n"
        "Transfer-Encoding: chunked\r\nConnection: Close\r\n\r\n";

    SECTION("a body of three chunks") {
        Request req;
        req.body = body_of(2500);
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.headers == head );
        // What is left of the stream buffer, less room for the size line of
        // the longest chunk that fits, "3f9\r\n" in a buffer of 1024, with
        // the CRLF after the data: after the head, 1024 - 93 - 5 - 2; then a
        // whole buffer; then the rest of the buffer after the last data,
        // which the read callback ends.
        CHECK( sent.read_sizes == std::vector<size_t>({ 924, 1017, 1017, 451 }) );
        REQUIRE( sent.sends.size() == 3 );
        CHECK( sent.sends[0] == head + chunk("39c", req.body.substr(0, 924)) );
        CHECK( sent.sends[1] == chunk("3f9", req.body.substr(924, 1017)) );
        CHECK( sent.sends[2] == chunk("22f", req.body.substr(1941)) + "0\r\n\r\n" );
    }

    SECTION("an empty body, in the send of the head") {
        Request req;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.sends == std::vector<std::string>({ head + "0\r\n\r\n" }) );
    }

    SECTION("a small body, and the chunk that ends it, in the send of the head") {
        Request req;
        req.body = "{\"power\":true}";
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.sends == std::vector<std::string>({
            head + chunk("e", req.body) + "0\r\n\r\n" }) );
    }

    SECTION("a read callback that gives a piece at a time, each a chunk of the same send") {
        Request req;
        req.body = "abcdef";
        req.piece = 2;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.sends == std::vector<std::string>({
            head + chunk("2", "ab") + chunk("2", "cd") + chunk("2", "ef") + "0\r\n\r\n" }) );
    }

    SECTION("a stream buffer with room for a byte of data") {
        Request req;
        req.body = "abc";
        req.stream_buff_size = 6;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.body_sends == std::vector<std::string>({
            "1\r\na\r\n", "1\r\nb\r\n", "1\r\nc\r\n", "0\r\n\r\n" }) );
    }

    SECTION("a stream buffer with no room for data") {
        Request req;
        req.body = "abc";
        req.stream_buff_size = 5;
        Sent sent = perform(req);
        CHECK( sent.res == KHC_ERR_TOO_LARGE_DATA );
        CHECK( sent.body_sends.empty() );
        CHECK( sent.closes == 1 );
    }

    SECTION("a read callback that gives more than it was asked for") {
        Request req;
        req.body = "abc";
        req.overrun = true;
        Sent sent = perform(req);
        CHECK( sent.res == KHC_ERR_READ_CALLBACK );
        CHECK( sent.body_sends.empty() );
        CHECK( sent.closes == 1 );
    }
}

TEST_CASE( "A request body of known length is sent with a Content-Length" ) {
    SECTION("in sends of up to the stream buffer, asking for no more than is left") {
        Request req;
        req.body = body_of(2500);
        req.body_length = 2500;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( has(sent.headers, "Content-Length: 2500\r\n") );
        CHECK_FALSE( has(sent.headers, "Transfer-Encoding") );
        // After the head, 87 bytes, the rest of the buffer; then a whole one.
        CHECK( sent.read_sizes == std::vector<size_t>({ 937, 1024, 539 }) );
        CHECK( sent.body_sends == std::vector<std::string>({
            req.body.substr(0, 937), req.body.substr(937, 1024), req.body.substr(1961) }) );
        REQUIRE( sent.sends.size() == 3 );
        CHECK( sent.sends[0] == sent.headers + req.body.substr(0, 937) );
    }

    SECTION("of 0, without asking the read callback") {
        Request req;
        req.body = "abc";
        req.body_length = 0;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( has(sent.headers, "Content-Length: 0\r\n") );
        CHECK( sent.read_sizes.empty() );
        CHECK( sent.body_sends.empty() );
    }

    SECTION("which the read callback ends early") {
        Request req;
        req.body = "abc";
        req.body_length = 10;
        Sent sent = perform(req);
        CHECK( sent.res == KHC_ERR_READ_CALLBACK );
        CHECK( sent.read_sizes == std::vector<size_t>({ 10, 7 }) );
        // Read in the buffer with the head, nothing of it was sent.
        CHECK( sent.sends.empty() );
        CHECK( sent.closes == 1 );
    }

    SECTION("which the read callback overruns") {
        Request req;
        req.body = "abc";
        req.body_length = 3;
        req.overrun = true;
        Sent sent = perform(req);
        CHECK( sent.res == KHC_ERR_READ_CALLBACK );
        CHECK( sent.body_sends.empty() );
    }

    SECTION("but not for a method without a body") {
        Request req;
        req.method = "GET";
        req.body = "abc";
        req.body_length = 3;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK_FALSE( has(sent.headers, "Content-Length") );
        CHECK( sent.read_sizes.empty() );
        CHECK( sent.body_sends.empty() );
    }
}

TEST_CASE( "The request body's length is part of the request" ) {
    khc_t http;
    khc_init(&http);
    CHECK( http._req_body_length == KHC_REQ_BODY_LENGTH_UNKNOWN );
    khc_set_req_body_length(&http, 42);
    khc_reset_except_request(&http);
    CHECK( http._req_body_length == 42 );
    khc_reset_except_cb(&http);
    CHECK( http._req_body_length == KHC_REQ_BODY_LENGTH_UNKNOWN );
}

// The host, the path and the method are sent as given, in the request line and
// the Host header, and each header as its own line. A space, or a CR and LF,
// in one of them made the request another: a path that ended early, a header
// that wasn't meant, or a second request after the first.
TEST_CASE( "khc refuses what would break the request line or a header" ) {
    khc_t http;
    khc_init(&http);
    REQUIRE( khc_set_host(&http, "api.kii.com") == KHC_ERR_OK );
    REQUIRE( khc_set_method(&http, "GET") == KHC_ERR_OK );
    REQUIRE( khc_set_path(&http, "/api/apps?x=%20y") == KHC_ERR_OK );

    SECTION("in the host, the path or the method, which stay as they were") {
        for (const char* bad : { "a b", "a\tb", "a\rb", "a\nb", "a\r\nX: y", "a\x7f", "a\x01" }) {
            CAPTURE( std::string(bad) );
            CHECK( khc_set_host(&http, bad) == KHC_ERR_FAIL );
            CHECK( khc_set_path(&http, bad) == KHC_ERR_FAIL );
            CHECK( khc_set_method(&http, bad) == KHC_ERR_FAIL );
        }
        CHECK( std::string(http._host) == "api.kii.com" );
        CHECK( std::string(http._path) == "/api/apps?x=%20y" );
        CHECK( std::string(http._method) == "GET" );
        // Bytes from 0x80 up, as of UTF-8, are sent as they are.
        CHECK( khc_set_path(&http, "/caf\xc3\xa9") == KHC_ERR_OK );
        // Still too large before anything else.
        const std::string long_path(KHC_PATH_MAX_SIZE, ' ');
        CHECK( khc_set_path(&http, long_path.c_str()) == KHC_ERR_TOO_LARGE_DATA );
    }

    SECTION("in a header, without connecting") {
        khct::cb::SockCtx s_ctx;
        khct::cb::IOCtx io_ctx;
        int connects = 0;
        s_ctx.on_connect = [&connects](void*, const char*, uint16_t) {
            ++connects;
            return EBISU_SOCK_FAIL;
        };
        khc_set_socket(&http, s_ctx.socket());
        khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
        khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);
        for (const char* bad : { "X-A: 1\r\nX-B: 2", "X-A: 1\nX-B: 2", "X-A: 1\r", "X-A: 1\r\n\r\nGET / HTTP/1.1" }) {
            CAPTURE( std::string(bad) );
            khc_slist_t* headers = khc_slist_append(NULL, "X-Good: 1", 9);
            headers = khc_slist_append(headers, bad, strlen(bad));
            khc_set_req_headers(&http, headers);
            CHECK( khc_perform(&http) == KHC_ERR_FAIL );
            khc_slist_free_all(headers);
        }
        CHECK( connects == 0 );
        // Ones without go on to connect.
        khc_slist_t* headers = khc_slist_append(NULL, "X-Good: a b\tc", 13);
        khc_set_req_headers(&http, headers);
        CHECK( khc_perform(&http) == KHC_ERR_SOCK_CONNECT );
        khc_slist_free_all(headers);
        CHECK( connects == 1 );
    }
}
