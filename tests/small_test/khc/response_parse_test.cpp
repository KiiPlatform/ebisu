// How khc reads a response's status line, headers and body, byte by byte:
// what it takes and what it refuses, wherever the reads end.
#include "catch.hpp"
#include <khc.h>
#include "test_callbacks.h"
#include <stdlib.h>
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
        }
        // A response refused gives the lines before the one refused, and none
        // of its body.
        CHECK( out.headers == expected.headers );
        CHECK( out.body == expected.body );
    }
}

void check_refused(const std::string& response, const std::vector<std::string>& headers_before,
        size_t header_buff_size = 256) {
    check_every_split(response, { KHC_ERR_INVALID_RESPONSE, 0, headers_before, "" },
            header_buff_size);
}

const std::string NUL(1, '\0');

}

// A header line used to end at the first CRLF strstr() found in the header
// buffer, which a NUL in the line hid. khc then read on until the buffer was
// full, and skipped what it held as one header too long for it: the headers
// after the NUL too, Content-Length among them, and, from a body or the next
// bytes, whatever came before the next CRLF. Lines end by length now, and a
// NUL is a control character no header or status line may hold.
TEST_CASE( "A NUL in a header is refused, and nothing after it is read" ) {
    const std::vector<std::string> status = { "HTTP/1.1 200 OK" };
    SECTION("in the middle of a value") {
        check_refused("HTTP/1.1 200 OK\r\nX-A: a" + NUL + "b\r\n"
                "Content-Length: 2\r\nX-B: 1\r\n\r\nok", status);
    }
    SECTION("in a buffer the response doesn't fit in") {
        // 40 bytes hold any one of these lines, and not two of them.
        check_refused("HTTP/1.1 200 OK\r\nX-A: " + NUL + "\r\n"
                "Content-Length: 0\r\n\r\n"
                "HTTP/1.1 200 OK\r\nContent-Length: 30\r\nX-Pad: 0123456789\r\n\r\n"
                "the body is not what it seems!", status, 40);
    }
    SECTION("right before the CRLF") {
        check_refused("HTTP/1.1 200 OK\r\nX-A: a" + NUL + "\r\nContent-Length: 2\r\n\r\nok", status);
    }
    SECTION("alone on a line") {
        check_refused("HTTP/1.1 200 OK\r\n" + NUL + "\r\nContent-Length: 2\r\n\r\nok", status);
    }
    SECTION("in a name") {
        check_refused("HTTP/1.1 200 OK\r\nX" + NUL + "A: a\r\nContent-Length: 2\r\n\r\nok", status);
    }
    SECTION("in a header too long for the buffer") {
        check_refused("HTTP/1.1 200 OK\r\nX-Long: " + NUL + std::string(100, 'x') +
                "\r\nContent-Length: 2\r\n\r\nok", status, 40);
        check_refused("HTTP/1.1 200 OK\r\nX-Long: " + std::string(100, 'x') + NUL +
                "\r\nContent-Length: 2\r\n\r\nok", status, 40);
    }
    SECTION("of an interim response") {
        check_refused("HTTP/1.1 100 Continue\r\nX-A: " + NUL + "\r\n\r\n"
                "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok", { "HTTP/1.1 100 Continue" });
    }
}

// A status line was found with strstr() too: one held back by a NUL ran the
// header buffer full, and failed as too large.
TEST_CASE( "A NUL in the status line is refused" ) {
    check_refused("HTTP/1.1 204 " + NUL + "\r\n\r\n", {});
    check_refused("HTTP/1.1 200 O" + NUL + "K\r\nContent-Length: 2\r\n\r\nok", {});
}

// A status line was "HTTP/", anything for the version, and three digits at the
// right place, with anything after them.
TEST_CASE( "Status lines that are HTTP/1.x" ) {
    const struct {
        std::string line;
        uint16_t status;
    } lines[] = {
        { "HTTP/1.1 200 OK", 200 },
        { "HTTP/1.0 200 OK", 200 },
        { "HTTP/1.9 200 OK", 200 },
        { "HTTP/1.1 404 Not Found", 404 },
        { "HTTP/1.1 599 Whatever It Says", 599 },
        { "HTTP/1.1 999 X", 999 },
        { "HTTP/1.1 000 X", 0 },
        // No reason phrase, with or without the space before it.
        { "HTTP/1.1 200 ", 200 },
        { "HTTP/1.1 200", 200 },
        // A reason phrase may hold spaces, tabs, any visible character and
        // bytes from 0x80 up.
        { "HTTP/1.1 200 O K", 200 },
        { "HTTP/1.1 200 \tOK\t", 200 },
        { "HTTP/1.1 200  OK", 200 },
        { "HTTP/1.1 200 !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", 200 },
        { "HTTP/1.1 200 \xc3\x89t\xc3\xa9", 200 },
        { "HTTP/1.1 200 \x80\xff", 200 },
    };
    for (const auto& l : lines) {
        check_every_split(l.line + "\r\nContent-Length: 2\r\n\r\nok",
                { KHC_ERR_OK, l.status, { l.line, "Content-Length: 2" }, "ok" });
    }
}

TEST_CASE( "Status lines that aren't one are refused" ) {
    const std::string lines[] = {
        // Not HTTP/1.x.
        "HTTP/x.y 200 OK",
        "HTTP/1 200 OK",
        "HTTP/1. 200 OK",
        "HTTP/1.10 200 OK",
        "HTTP/11 200 OK",
        "HTTP/2.0 200 OK",
        "HTTP/0.9 200 OK",
        "HTTP/9.9 200 OK",
        "http/1.1 200 OK",
        "Http/1.1 200 OK",
        "HTTPS/1.1 200 OK",
        "ICY 200 OK",
        "SIP/2.0 200 OK",
        "RTSP/1.0 200 OK",
        " HTTP/1.1 200 OK",
        "\tHTTP/1.1 200 OK",
        "HTTP/1.1",
        "HTTP/1.1 ",
        // Not a space and three digits.
        "HTTP/1.1  200 OK",
        "HTTP/1.1\t200 OK",
        "HTTP/1.1 2000 OK",
        "HTTP/1.1 20 OK",
        "HTTP/1.1 2 OK",
        "HTTP/1.1 20",
        "HTTP/1.1 2x0 OK",
        "HTTP/1.1 abc OK",
        "HTTP/1.1 -20 OK",
        "HTTP/1.1 +20 OK",
        "HTTP/1.1 2\xb9" "0 OK",
        // Not a space after them.
        "HTTP/1.1 200OK",
        "HTTP/1.1 200\tOK",
        "HTTP/1.1 200-OK",
        // Control characters in the reason phrase.
        "HTTP/1.1 200 O\x01K",
        "HTTP/1.1 200 O\x1fK",
        "HTTP/1.1 200 O\x7fK",
        "HTTP/1.1 200 O\rK",
        "HTTP/1.1 200 O\nK",
        "HTTP/1.1 200 OK\r",
        "HTTP/1.1 200 OK\n",
        "HTTP/1.1 200 O\x0bK",
        "HTTP/1.1 200 O\x0cK",
        "HTTP/1.1 200 \x1b[0m",
        // An empty line before it.
        "",
    };
    for (const auto& line : lines) {
        check_refused(line + "\r\nContent-Length: 2\r\n\r\nok", {});
    }
}

namespace {

// Each of headers alone in a response, which khc takes and gives as it is.
void check_headers_taken(const std::vector<std::string>& headers) {
    for (const auto& header : headers) {
        check_every_split("HTTP/1.1 200 OK\r\n" + header + "\r\nContent-Length: 2\r\n\r\nok",
                { KHC_ERR_OK, 200, { "HTTP/1.1 200 OK", header, "Content-Length: 2" }, "ok" });
    }
}

// Each of headers alone in a response, which khc refuses, having given the
// status line only.
void check_headers_refused(const std::vector<std::string>& headers) {
    for (const auto& header : headers) {
        check_refused("HTTP/1.1 200 OK\r\n" + header + "\r\nContent-Length: 2\r\n\r\nok",
                { "HTTP/1.1 200 OK" });
    }
}

}

TEST_CASE( "Headers are a name, a colon and a value" ) {
    check_headers_taken({
        "Content-Type: application/json",
        "X-A:b",
        "X-A: ",
        "X-A:",
        "X-A:\tb",
        "X-A: b \t",
        "X-A: \t b",
        "X-A: a b\tc",
        "X-A: a:b: c",
        "X-A: \"quoted, with a comma\"",
        "X-A: caf\xc3\xa9",
        "X-A: \x80\xff",
        "X-A: !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
        // Every character a name may hold.
        "!#$%&'*+-.^_`|~0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ: v",
        "x: v",
        "9: v",
        "ETag: \"abc\"",
        "Date: Sun, 06 Nov 1994 08:49:37 GMT",
    });
}

// A header whose name isn't one, or whose value holds a control character, can
// be read another way by something else on the way, which is how responses
// are split and smuggled. llhttp refuses each of these too.
TEST_CASE( "Headers that aren't one are refused" ) {
    SECTION("with no name") {
        check_headers_refused({ ": b", ":", ":b" });
    }
    SECTION("with no colon") {
        check_headers_refused({ "X-A", "X-A b", "garbage", "Content-Length 2" });
    }
    SECTION("with whitespace before the colon") {
        check_headers_refused({
            "X-A : b", "X-A\t: b", "X-A  : b",
            "Content-Length : 2", "Transfer-Encoding : chunked", "Content-Length\t: 2",
        });
    }
    SECTION("with a character a name can't hold") {
        check_headers_refused({
            "X A: b", "X\tA: b", "X\"A: b", "X(A): b", "X)A: b", "X,A: b", "X/A: b",
            "X;A: b", "X<A: b", "X=A: b", "X>A: b", "X?A: b", "X@A: b", "X[A: b",
            "X\\A: b", "X]A: b", "X{A: b", "X}A: b", "X\x01" "A: b", "X\x7f" "A: b",
            "X\x80" "A: b", "\xc3\x89t\xc3\xa9: b",
        });
    }
    SECTION("continuing the line before, in the obsolete line folding") {
        check_refused("HTTP/1.1 200 OK\r\nX-A: a\r\n b\r\nContent-Length: 2\r\n\r\nok",
                { "HTTP/1.1 200 OK", "X-A: a" });
        check_refused("HTTP/1.1 200 OK\r\nX-A: a\r\n\tb\r\nContent-Length: 2\r\n\r\nok",
                { "HTTP/1.1 200 OK", "X-A: a" });
        check_refused("HTTP/1.1 200 OK\r\n Content-Length: 2\r\n\r\nok", { "HTTP/1.1 200 OK" });
    }
    SECTION("with a control character in the value") {
        std::vector<std::string> headers;
        for (int c = 0x00; c < 0x20; ++c) {
            if (c != '\t') {
                headers.push_back("X-A: a" + std::string(1, (char)c) + "b");
            }
        }
        headers.push_back("X-A: a\x7f" "b");
        check_headers_refused(headers);
    }
    SECTION("with a bare CR or LF") {
        check_headers_refused({
            "X-A: a\nContent-Length: 3",
            "X-A: a\rContent-Length: 3",
            "X-A: a\n",
            "X-A: a\r",
            "X-A:\n b",
            "\nX-A: b",
            "\rX-A: b",
        });
    }
}

TEST_CASE( "An interim response's headers are checked as a final one's" ) {
    check_refused("HTTP/1.1 100 Continue\r\nX-A : b\r\n\r\nHTTP/1.1 200 OK\r\n\r\nok",
            { "HTTP/1.1 100 Continue" });
    check_refused("HTTP/1.1 100 Continue\r\n\r\nHTTP/x.y 200 OK\r\n\r\nok",
            { "HTTP/1.1 100 Continue" });
    check_refused("HTTP/1.1 103 Early Hints\r\nLink: a\r\n b\r\n\r\nHTTP/1.1 200 OK\r\n\r\nok",
            { "HTTP/1.1 103 Early Hints", "Link: a" });
}

// A header too long for the buffer is skipped, but what comes of it has to be
// a header all the same.
TEST_CASE( "A header too long for the buffer is checked as it is skipped" ) {
    const std::string tail = "\r\nContent-Length: 2\r\n\r\nok";
    const std::string x100(100, 'x');
    SECTION("one that is a header is skipped") {
        for (const std::string& header : {
                "X-Long: " + x100,
                "X-Long:" + x100,
                "X-Long: " + x100 + " \t",
                "X-Long: " + std::string(37, 'x'),
                "X-Long: " + std::string(38, 'x'),
                "X-Long: " + std::string(39, 'x'),
                "X-Long: " + std::string(40, 'x') + "\xc3\xa9" }) {
            for (const auto& reads : splits_of("HTTP/1.1 200 OK\r\n" + header + tail)) {
                Reply reply;
                reply.reads = reads;
                reply.header_buff_size = 40;
                Outcome out = perform(reply);
                INFO( "header " << printable(header) );
                INFO( reads.size() << " reads, the first of " << reads[0].size() << " bytes" );
                CHECK( out.res == KHC_ERR_OK );
                CHECK( out.skipped );
                CHECK( out.headers == std::vector<std::string>{ "HTTP/1.1 200 OK", "Content-Length: 2" } );
                CHECK( out.body == "ok" );
            }
        }
    }
    SECTION("one that isn't is refused, in the part that fits or in the rest") {
        for (const std::string& header : {
                "X Long: " + x100,
                "X-Long : " + x100,
                " X-Long: " + x100,
                "X-Long" + x100,
                "X-Long: \x01" + x100,
                "X-Long: " + x100 + "\x01",
                "X-Long: " + x100 + "\x7f",
                "X-Long: " + x100 + "\n" + "Content-Length: 3",
                "X-Long: " + x100 + "\r" + "Content-Length: 3",
                "X-Long: " + std::string(38, 'x') + "\r" + x100,
                "X-Long: " + std::string(38, 'x') + "\n" + x100 }) {
            INFO( "header " << printable(header) );
            check_refused("HTTP/1.1 200 OK\r\n" + header + tail, { "HTTP/1.1 200 OK" }, 40);
        }
    }
}

namespace {

const std::string CHUNKED_200 = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";
const std::vector<std::string> CHUNKED_HEADERS = { "HTTP/1.1 200 OK", "Transfer-Encoding: chunked" };

// A chunked body refused, having given body of it, through stream buffers
// small and large, wherever the reads end.
void check_chunks_refused(const std::string& chunks, const std::string& body_before) {
    for (size_t stream_buff_size : { 8, 16, 256 }) {
        for (const auto& reads : splits_of(CHUNKED_200 + chunks)) {
            Reply reply;
            reply.reads = reads;
            reply.stream_buff_size = stream_buff_size;
            Outcome out = perform(reply);
            INFO( "chunks " << printable(chunks) << ", stream buffer of " << stream_buff_size );
            INFO( reads.size() << " reads, the first of " << reads[0].size() << " bytes" );
            CHECK( out.res == KHC_ERR_INVALID_RESPONSE );
            CHECK( out.headers == CHUNKED_HEADERS );
            CHECK( out.body == body_before );
        }
    }
}

// A chunk size line has to fit in the stream buffer: stream_buff_sizes are
// those it fits in.
void check_chunks_taken(const std::string& chunks, const std::string& body,
        const std::vector<size_t>& stream_buff_sizes = { 8, 16, 256 }) {
    for (size_t stream_buff_size : stream_buff_sizes) {
        for (const auto& reads : splits_of(CHUNKED_200 + chunks)) {
            Reply reply;
            reply.reads = reads;
            reply.stream_buff_size = stream_buff_size;
            Outcome out = perform(reply);
            INFO( "chunks " << printable(chunks) << ", stream buffer of " << stream_buff_size );
            INFO( reads.size() << " reads, the first of " << reads[0].size() << " bytes" );
            CHECK( out.res == KHC_ERR_OK );
            CHECK( out.body == body );
        }
    }
}

}

// The two bytes after a chunk's data were skipped, whatever they were: a
// chunk whose size was wrong went on as if it had been right.
TEST_CASE( "A chunk's data ends with a CRLF, or the response is refused" ) {
    for (const std::string& after : std::vector<std::string>{ "XX", "\r\r", "\n\n", "\rX", "X\n",
            "\n\r", " \r", "\r ", std::string("\0\0", 2) }) {
        check_chunks_refused("5\r\nhello" + after + "3\r\nabc\r\n0\r\n\r\n", "hello");
        check_chunks_refused("5\r\nhello" + after + "0\r\n\r\n", "hello");
    }
    // A size one short, and one long.
    check_chunks_refused("4\r\nhello\r\n0\r\n\r\n", "hell");
    check_chunks_refused("6\r\nhello\r\n0\r\n\r\n", "hello\r");
    // A bare LF in place of the CRLF.
    check_chunks_refused("5\r\nhello\n0\r\n\r\n", "hello");
    // After a chunk read whole, and then the bad one.
    check_chunks_refused("3\r\nabc\r\n5\r\nhelloXX0\r\n\r\n", "abchello");
}

TEST_CASE( "Chunks that end where their sizes say are taken" ) {
    check_chunks_taken("5\r\nhello\r\n0\r\n\r\n", "hello");
    check_chunks_taken("5\r\nhello\r\n3\r\nabc\r\n0\r\n\r\n", "helloabc");
    check_chunks_taken("1\r\n\r\r\n1\r\n\n\r\n0\r\n\r\n", "\r\n");
    check_chunks_taken("2\r\n\r\n\r\n0\r\n\r\n", "\r\n");
    check_chunks_taken("5 ;a=b\r\nhello\r\n0\r\n\r\n", "hello");
    check_chunks_taken("5;a=\"x y\"\r\nhello\r\n0;z\r\n\r\n", "hello", { 16, 256 });
}

TEST_CASE( "A chunk size followed by whitespace and nothing else is refused" ) {
    check_chunks_refused("5 \r\nhello\r\n0\r\n\r\n", "");
    check_chunks_refused("5\t\r\nhello\r\n0\r\n\r\n", "");
    check_chunks_refused("5\r\nhello\r\n0 \r\n\r\n", "hello");
}

TEST_CASE( "Chunks and a Content-Length together are refused" ) {
    for (const std::string& response : std::vector<std::string>{
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 5\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
            "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n",
            "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" }) {
        Expected refused = { KHC_ERR_INVALID_RESPONSE, 0, {}, "" };
        for (const auto& reads : splits_of(response)) {
            Reply reply;
            reply.reads = reads;
            Outcome out = perform(reply);
            INFO( "response " << printable(response) );
            CHECK( out.res == refused.res );
            CHECK( out.body == "" );
        }
    }
    SECTION("but not where there is no body") {
        for (const std::string& status : std::vector<std::string>{ "204 No Content", "304 Not Modified" }) {
            check_every_split("HTTP/1.1 " + status + "\r\nContent-Length: 5\r\n"
                    "Transfer-Encoding: chunked\r\n\r\n",
                    { KHC_ERR_OK, (uint16_t)atoi(status.c_str()),
                      { "HTTP/1.1 " + status, "Content-Length: 5", "Transfer-Encoding: chunked" }, "" });
        }
        Reply reply;
        reply.method = "HEAD";
        reply.reads = { "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n" };
        CHECK( perform(reply).res == KHC_ERR_OK );
    }
}

// HTTP/1.0 has no chunks: RFC 9112 has a message of it with Transfer-Encoding
// treated as if its framing were faulty, which it likely is, a proxy having
// passed chunks on that it didn't decode.
TEST_CASE( "Chunks in an HTTP/1.0 response are refused" ) {
    for (const auto& reads : splits_of(
            "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n")) {
        Reply reply;
        reply.reads = reads;
        Outcome out = perform(reply);
        CHECK( out.res == KHC_ERR_INVALID_RESPONSE );
        CHECK( out.body == "" );
    }
    SECTION("while its Content-Length, or none, frames it") {
        check_every_split("HTTP/1.0 200 OK\r\nContent-Length: 5\r\n\r\nhello",
                { KHC_ERR_OK, 200, { "HTTP/1.0 200 OK", "Content-Length: 5" }, "hello" });
        check_every_split("HTTP/1.0 200 OK\r\n\r\nhello",
                { KHC_ERR_OK, 200, { "HTTP/1.0 200 OK" }, "hello" });
    }
    SECTION("but not where there is no body") {
        Reply reply;
        reply.method = "HEAD";
        reply.reads = { "HTTP/1.0 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" };
        CHECK( perform(reply).res == KHC_ERR_OK );
    }
}
