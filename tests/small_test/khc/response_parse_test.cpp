// How khc reads a response's status line, headers and body, byte by byte:
// what it takes and what it refuses, wherever the reads end.
#include "catch.hpp"
#include <khc.h>
#include "test_callbacks.h"
#include <string>
#include <vector>

namespace {

struct Reply {
    std::vector<std::string> reads;
    std::string method = "GET";
    size_t header_buff_size = 256;
    size_t stream_buff_size = 256;
};

struct Outcome {
    khc_code res;
    uint16_t status;
    std::vector<std::string> headers;
    std::string body;
    bool skipped;
    // Reads asked for once the reply was over: the connection then closes.
    int reads_after_end;
};

Outcome perform(const Reply& reply) {
    std::vector<char> header_buff(reply.header_buff_size);
    std::vector<char> stream_buff(reply.stream_buff_size);
    khc http;
    khc_init(&http);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, reply.method.c_str());
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_resp_header_buff(&http, header_buff.data(), header_buff.size());
    khc_set_stream_buff(&http, stream_buff.data(), stream_buff.size());

    khct::cb::SockCtx s_ctx;
    khc_set_socket(&http, s_ctx.socket());
    khct::cb::IOCtx io_ctx;
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    Outcome out = {};
    s_ctx.on_connect = [](void*, const char*, uint16_t) {
        return EBISU_SOCK_OK;
    };
    s_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
        *out_sent_length = length;
        return EBISU_SOCK_OK;
    };
    size_t next = 0;
    size_t offset = 0;
    s_ctx.on_recv = [&](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        *out_actual_length = 0;
        if (next == reply.reads.size()) {
            ++out.reads_after_end;
            return EBISU_SOCK_OK;
        }
        *out_actual_length = reply.reads[next].copy(buffer, length_to_read, offset);
        offset += *out_actual_length;
        if (offset == reply.reads[next].size()) {
            ++next;
            offset = 0;
        }
        return EBISU_SOCK_OK;
    };
    s_ctx.on_close = [](void*) {
        return EBISU_SOCK_OK;
    };
    io_ctx.on_header = [&out](char* buffer, size_t size, void*) {
        out.headers.push_back(std::string(buffer, size));
        return size;
    };
    io_ctx.on_write = [&out](char* buffer, size_t size, void*) {
        out.body.append(buffer, size);
        return size;
    };

    out.res = khc_perform(&http);
    out.status = khc_get_status_code(&http);
    out.skipped = khc_get_resp_header_skipped(&http);
    return out;
}

// The response in one read, split in two wherever it can be, and a byte at a
// time.
std::vector<std::vector<std::string>> splits_of(const std::string& response) {
    std::vector<std::vector<std::string>> splits;
    splits.push_back({ response });
    for (size_t i = 1; i < response.size(); ++i) {
        splits.push_back({ response.substr(0, i), response.substr(i) });
    }
    std::vector<std::string> bytes;
    for (char c : response) {
        bytes.push_back(std::string(1, c));
    }
    splits.push_back(bytes);
    return splits;
}

// What a test expects of a response, however it is read.
struct Expected {
    khc_code res;
    uint16_t status;
    std::vector<std::string> headers;
    std::string body;
};

// Bytes as a test can print them, a NUL included.
std::string printable(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c >= 0x20 && c < 0x7f) {
            out += (char)c;
        } else {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%02x", c);
            out += hex;
        }
    }
    return out;
}

void check_every_split(const std::string& response, const Expected& expected,
        size_t header_buff_size = 256) {
    for (const auto& reads : splits_of(response)) {
        Reply reply;
        reply.reads = reads;
        reply.header_buff_size = header_buff_size;
        Outcome out = perform(reply);
        INFO( "response " << printable(response) );
        INFO( reads.size() << " reads, the first of " << reads[0].size() << " bytes" );
        CHECK( out.res == expected.res );
        if (expected.res == KHC_ERR_OK) {
            CHECK( out.status == expected.status );
            CHECK( out.headers == expected.headers );
            CHECK( out.body == expected.body );
        }
    }
}

const std::string NUL(1, '\0');

}

// A header line used to end at the first CRLF strstr() found in the header
// buffer, which a NUL in the line hid. khc then read on until the buffer was
// full, and skipped what it held as one header too long for it: the headers
// after the NUL too, Content-Length among them, and, from a body or the next
// bytes, whatever came before the next CRLF.
TEST_CASE( "A NUL in a header doesn't end the header buffer" ) {
    SECTION("the headers after it are read") {
        const std::string response = "HTTP/1.1 200 OK\r\nX-A: a" + NUL + "b\r\n"
            "Content-Length: 2\r\nX-B: 1\r\n\r\nok";
        check_every_split(response, { KHC_ERR_OK, 200,
                { "HTTP/1.1 200 OK", "X-A: a" + NUL + "b", "Content-Length: 2", "X-B: 1" },
                "ok" });
    }
    SECTION("in a buffer the response doesn't fit in") {
        // 40 bytes hold any one of these lines, and not two of them.
        const std::string response = "HTTP/1.1 200 OK\r\nX-A: " + NUL + "\r\n"
            "Content-Length: 0\r\n\r\n"
            "HTTP/1.1 200 OK\r\nContent-Length: 30\r\nX-Pad: 0123456789\r\n\r\n"
            "the body is not what it seems!";
        check_every_split(response, { KHC_ERR_OK, 200,
                { "HTTP/1.1 200 OK", "X-A: " + NUL, "Content-Length: 0" }, "" }, 40);
    }
    SECTION("right before the CRLF, and alone") {
        const std::string response = "HTTP/1.1 200 OK\r\nX-A: a" + NUL + "\r\n"
            + NUL + "\r\nContent-Length: 2\r\n\r\nok";
        check_every_split(response, { KHC_ERR_OK, 200,
                { "HTTP/1.1 200 OK", "X-A: a" + NUL, NUL, "Content-Length: 2" }, "ok" });
    }
    SECTION("in the status line") {
        const std::string response = "HTTP/1.1 200 O" + NUL + "K\r\nContent-Length: 2\r\n\r\nok";
        check_every_split(response, { KHC_ERR_OK, 200,
                { "HTTP/1.1 200 O" + NUL + "K", "Content-Length: 2" }, "ok" });
    }
    SECTION("in a header too long for the buffer, which is skipped alone") {
        const std::string response = "HTTP/1.1 200 OK\r\nX-Long: " + NUL
            + std::string(100, 'x') + "\r\nContent-Length: 2\r\n\r\nok";
        for (const auto& reads : splits_of(response)) {
            Reply reply;
            reply.reads = reads;
            reply.header_buff_size = 40;
            Outcome out = perform(reply);
            INFO( reads.size() << " reads, the first of " << reads[0].size() << " bytes" );
            CHECK( out.res == KHC_ERR_OK );
            CHECK( out.skipped );
            CHECK( out.headers == std::vector<std::string>{ "HTTP/1.1 200 OK", "Content-Length: 2" } );
            CHECK( out.body == "ok" );
        }
    }
    SECTION("of an interim response") {
        const std::string response = "HTTP/1.1 100 Continue\r\nX-A: " + NUL + "\r\n\r\n"
            "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        check_every_split(response, { KHC_ERR_OK, 200,
                { "HTTP/1.1 100 Continue", "X-A: " + NUL, "HTTP/1.1 200 OK", "Content-Length: 2" },
                "ok" });
    }
}

// A status line was found with strstr() too: one held back by a NUL ran the
// header buffer full, and failed as too large.
TEST_CASE( "A NUL before the status line's CRLF doesn't hide it" ) {
    const std::string response = "HTTP/1.1 204 " + NUL + "\r\n\r\n";
    check_every_split(response, { KHC_ERR_OK, 204, { "HTTP/1.1 204 " + NUL }, "" });
}
