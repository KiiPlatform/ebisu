#include "catch.hpp"
#include <khc.h>
#include "test_callbacks.h"
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
};

struct Sent {
    khc_code res;
    // Up to the blank line that ends them.
    std::string headers;
    // Each send after the headers.
    std::vector<std::string> body_sends;
    // What the read callback was asked for, each time.
    std::vector<size_t> read_sizes;
    int closes;
};

// Performs req, answered with 204, and records what khc sent.
Sent perform(const Request& req) {
    std::vector<char> stream_buff(req.stream_buff_size);
    khc http;
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
        size_t n = req.body.copy(buffer, size, read_offset);
        read_offset += n;
        return req.overrun && n > 0 ? size + 1 : n;
    };

    sent.res = khc_perform(&http);

    size_t i = 0;
    for (; i < sends.size(); ++i) {
        sent.headers += sends[i];
        if (sent.headers.find("\r\n\r\n") != std::string::npos) {
            ++i;
            break;
        }
    }
    sent.body_sends.assign(sends.begin() + i, sends.end());
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
// over TLS, three records for each chunk.
TEST_CASE( "Each chunk of a request body is sent in one write" ) {
    SECTION("a body of three chunks") {
        Request req;
        req.body = body_of(2500);
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( has(sent.headers, "Transfer-Encoding: chunked\r\n") );
        // The stream buffer less room for "3f9\r\n", the size line of the
        // longest chunk that fits, and the CRLF after it.
        CHECK( sent.read_sizes == std::vector<size_t>({ 1017, 1017, 1017, 1017 }) );
        REQUIRE( sent.body_sends.size() == 4 );
        CHECK( sent.body_sends[0] == chunk("3f9", req.body.substr(0, 1017)) );
        CHECK( sent.body_sends[1] == chunk("3f9", req.body.substr(1017, 1017)) );
        CHECK( sent.body_sends[2] == chunk("1d2", req.body.substr(2034)) );
        CHECK( sent.body_sends[3] == "0\r\n\r\n" );
    }

    SECTION("an empty body") {
        Request req;
        Sent sent = perform(req);
        REQUIRE( sent.res == KHC_ERR_OK );
        CHECK( sent.body_sends == std::vector<std::string>({ "0\r\n\r\n" }) );
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
        CHECK( sent.read_sizes == std::vector<size_t>({ 1024, 1024, 452 }) );
        CHECK( sent.body_sends == std::vector<std::string>({
            req.body.substr(0, 1024), req.body.substr(1024, 1024), req.body.substr(2048) }) );
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
        CHECK( sent.body_sends == std::vector<std::string>({ "abc" }) );
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
    khc http;
    khc_init(&http);
    CHECK( http._req_body_length == KHC_REQ_BODY_LENGTH_UNKNOWN );
    khc_set_req_body_length(&http, 42);
    khc_reset_except_request(&http);
    CHECK( http._req_body_length == 42 );
    khc_reset_except_cb(&http);
    CHECK( http._req_body_length == KHC_REQ_BODY_LENGTH_UNKNOWN );
}
