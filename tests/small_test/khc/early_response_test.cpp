#include "catch_amalgamated.hpp"
#include <khc.h>
#include "test_callbacks.h"

#include <string>
#include <string.h>
#include <vector>

// A server may answer a request before its body is all sent, with a 401 or a
// 413 for instance, and close the connection. RFC 9112, section 9.5: "A client
// sending a message body SHOULD monitor the network connection for an error
// response while it is transmitting the request. If the client sees a
// response that indicates the server does not wish to receive the message
// body and is closing the connection, the client SHOULD immediately cease
// transmitting the body and close its side of the connection." khc sent the
// whole body before it read anything: on a connection the server had closed,
// a send failed, and the answer was lost with it.

namespace {

// A server that answers once it has received answer_after bytes of the
// request, closing the connection then if it closes: sends after that fail,
// as on a connection reset. recv_wait says whether the answer is there.
struct Server {
    khct::cb::SockCtx sock;
    std::string request;
    std::string answer;
    size_t answer_after;
    bool closes;
    // What the server sends after the answer, once the whole request is in,
    // for an answer that is an interim 1xx.
    std::string after_body;
    size_t request_length;
    size_t answered;
    size_t sends_failed;

    explicit Server(bool with_recv_wait = true) :
        answer_after(0), closes(true), request_length(0), answered(0), sends_failed(0) {
        if (with_recv_wait) {
            sock.ops.recv_wait = khct::cb::mock_recv_wait;
        }
        sock.on_connect = [](void*, const char*, uint16_t) { return EBISU_SOCK_OK; };
        sock.on_send = [this](void*, const char* buffer, size_t length, size_t* out_sent) {
            *out_sent = 0;
            if (closes && has_answered()) {
                ++sends_failed;
                return EBISU_SOCK_FAIL;
            }
            request.append(buffer, length);
            *out_sent = length;
            return EBISU_SOCK_OK;
        };
        sock.on_recv_wait = [this](uint32_t) { return has_answered() && answered < to_send().size(); };
        sock.on_recv = [this](void*, char* buffer, size_t length, size_t* out_received) {
            const std::string data = to_send();
            size_t n = data.size() - answered;
            if (n > length) {
                n = length;
            }
            memcpy(buffer, data.data() + answered, n);
            answered += n;
            *out_received = n;
            return EBISU_SOCK_OK;
        };
        sock.on_close = [](void*) { return EBISU_SOCK_OK; };
    }

    bool has_answered() const {
        return request.size() >= answer_after;
    }

    std::string to_send() const {
        if (!has_answered()) {
            return "";
        }
        if (request_length > 0 && request.size() >= request_length) {
            return answer + after_body;
        }
        return answer;
    }
};

struct Request {
    khc_t http;
    khct::cb::IOCtx io;
    std::string body;
    size_t read;
    std::string received;
    std::vector<char> stream;

    Request(Server& server, size_t body_size) : body(body_size, 'b'), read(0), stream(256) {
        khc_init(&http);
        khc_set_host(&http, "api.kii.com");
        khc_set_method(&http, "POST");
        khc_set_path(&http, "/upload");
        khc_set_req_body_length(&http, body.size());
        khc_set_stream_buff(&http, stream.data(), stream.size());
        khc_set_socket(&http, server.sock.socket());
        io.on_read = [this](char* buffer, size_t size, void*) {
            size_t n = body.size() - read;
            if (n > size) {
                n = size;
            }
            memcpy(buffer, body.data() + read, n);
            read += n;
            return n;
        };
        io.on_header = [](char*, size_t size, void*) { return size; };
        io.on_write = [this](char* buffer, size_t size, void*) {
            received.append(buffer, size);
            return size;
        };
        khc_set_cb_read(&http, khct::cb::cb_read, &io);
        khc_set_cb_write(&http, khct::cb::cb_write, &io);
        khc_set_cb_header(&http, khct::cb::cb_header, &io);
    }
};

const char FORBIDDEN[] =
    "HTTP/1.1 403 Forbidden\r\nContent-Length: 9\r\nConnection: close\r\n\r\nforbidden";

}

TEST_CASE( "khc reads a server's answer that comes before the body is sent, and stops sending" ) {
    Server server;
    server.answer = FORBIDDEN;
    // After the headers and part of the body.
    server.answer_after = 300;
    Request req(server, 10000);

    CHECK( khc_perform(&req.http) == KHC_ERR_OK );
    CHECK( khc_get_status_code(&req.http) == 403 );
    CHECK( req.received == "forbidden" );
    // The rest of the body wasn't sent, nor tried.
    CHECK( server.request.size() < 1000 );
    CHECK( server.sends_failed == 0 );
}

TEST_CASE( "khc reads a server's answer that a send finds the connection closed after" ) {
    Server server;
    server.answer = FORBIDDEN;
    Request req(server, 10000);
    // The answer comes as the send of the body's second part fails, so the
    // look before each part doesn't see it.
    size_t sends = 0;
    server.answer_after = SIZE_MAX;
    auto send = server.sock.on_send;
    server.sock.on_send = [&](void* ctx, const char* buffer, size_t length, size_t* out_sent) {
        if (++sends == 5) {
            server.answer_after = 0;
        }
        return send(ctx, buffer, length, out_sent);
    };

    CHECK( khc_perform(&req.http) == KHC_ERR_OK );
    CHECK( khc_get_status_code(&req.http) == 403 );
    CHECK( req.received == "forbidden" );
    CHECK( server.sends_failed == 1 );
}

TEST_CASE( "khc goes on sending the body after an interim response" ) {
    Server server;
    server.answer = "HTTP/1.1 100 Continue\r\n\r\n";
    server.after_body = "HTTP/1.1 201 Created\r\nContent-Length: 2\r\n\r\nok";
    server.answer_after = 300;
    server.closes = false;
    Request req(server, 10000);
    // The request is whole once the body is in: the headers, and the body.
    server.request_length = SIZE_MAX;
    auto send = server.sock.on_send;
    server.sock.on_send = [&](void* ctx, const char* buffer, size_t length, size_t* out_sent) {
        ebisu_sock_code_t res = send(ctx, buffer, length, out_sent);
        if (req.read == req.body.size() && server.request_length == SIZE_MAX &&
                server.request.size() >= req.body.size()) {
            server.request_length = server.request.size();
        }
        return res;
    };

    CHECK( khc_perform(&req.http) == KHC_ERR_OK );
    CHECK( khc_get_status_code(&req.http) == 201 );
    CHECK( req.received == "ok" );
    CHECK( req.read == req.body.size() );
    CHECK( server.request.size() == server.request_length );
}

// A send that failed, with an interim response waiting, left part of the
// stream buffer unsent: after the 1xx, sending goes on from there, so that
// the server gets the request once, whole.
TEST_CASE( "khc goes on from where a failed send stopped, after an interim response" ) {
    Server server;
    server.answer = "HTTP/1.1 100 Continue\r\n\r\n";
    server.after_body = "HTTP/1.1 201 Created\r\nContent-Length: 2\r\n\r\nok";
    server.closes = false;
    server.answer_after = SIZE_MAX;
    Request req(server, 1000);
    const std::string head = "POST /upload HTTP/1.1\r\nHOST: api.kii.com\r\n"
        "Content-Length: 1000\r\nConnection: Close\r\n\r\n";
    server.request_length = head.size() + req.body.size();
    // The first send takes 100 bytes of the buffer, the second fails with the
    // 1xx there to read.
    size_t sends = 0;
    auto send = server.sock.on_send;
    server.sock.on_send = [&](void* ctx, const char* buffer, size_t length, size_t* out_sent) {
        ++sends;
        if (sends == 1) {
            ebisu_sock_code_t res = send(ctx, buffer, 100, out_sent);
            return res;
        }
        if (sends == 2) {
            server.answer_after = 0;
            *out_sent = 0;
            return EBISU_SOCK_FAIL;
        }
        return send(ctx, buffer, length, out_sent);
    };

    CHECK( khc_perform(&req.http) == KHC_ERR_OK );
    CHECK( khc_get_status_code(&req.http) == 201 );
    CHECK( req.received == "ok" );
    CHECK( server.request == head + req.body );
}

TEST_CASE( "khc without recv_wait, or with nothing waiting, sends as it did" ) {
    SECTION("without recv_wait, a send that fails fails the request") {
        Server server(false);
        server.answer = FORBIDDEN;
        server.answer_after = 300;
        Request req(server, 10000);
        CHECK( khc_perform(&req.http) == KHC_ERR_SOCK_SEND );
        CHECK( khc_get_status_code(&req.http) == 0 );
    }
    SECTION("with nothing waiting, a send that fails fails the request") {
        Server server;
        server.answer = "";
        server.answer_after = 300;
        Request req(server, 10000);
        CHECK( khc_perform(&req.http) == KHC_ERR_SOCK_SEND );
    }
    SECTION("with nothing waiting until the body is sent, all of it is") {
        Server server;
        server.answer = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        server.closes = false;
        Request req(server, 10000);
        server.answer_after = SIZE_MAX;
        auto send = server.sock.on_send;
        server.sock.on_send = [&](void* ctx, const char* buffer, size_t length, size_t* out_sent) {
            ebisu_sock_code_t res = send(ctx, buffer, length, out_sent);
            if (req.read == req.body.size()) {
                server.answer_after = 0;
            }
            return res;
        };
        CHECK( khc_perform(&req.http) == KHC_ERR_OK );
        CHECK( khc_get_status_code(&req.http) == 200 );
        CHECK( req.read == req.body.size() );
    }
}
