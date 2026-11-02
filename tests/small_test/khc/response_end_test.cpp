#include "catch.hpp"
#include <khc.h>
#include "test_callbacks.h"
#include <string>
#include <vector>

namespace {

const std::string STATUS = "HTTP/1.1 200 OK\r\n";
const std::string CHUNKED = STATUS + "Transfer-Encoding: chunked\r\n\r\n";

struct Peer {
    std::vector<std::string> reads;
    // After the reads, whether the connection closes or just stays quiet.
    bool closes = false;
    std::string method = "GET";
    size_t stream_buff_size = 1024;
};

struct Result {
    khc_code res;
    std::string body;
    int timeouts;
    int on_close_called;
};

// Performs a request answered by peer. Past its reads, recv fails as a timeout
// on a connection left open would, unless the peer closes.
Result perform(const Peer& peer) {
    std::vector<char> stream_buff(peer.stream_buff_size);
    khc http;
    khc_init(&http);
    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, peer.method.c_str());
    khc_set_path(&http, "/api/apps");
    khc_set_req_headers(&http, NULL);
    khc_set_stream_buff(&http, stream_buff.data(), stream_buff.size());

    khct::cb::SockCtx s_ctx;
    khc_set_cb_sock_connect(&http, khct::cb::mock_connect, &s_ctx);
    khc_set_cb_sock_send(&http, khct::cb::mock_send, &s_ctx);
    khc_set_cb_sock_recv(&http, khct::cb::mock_recv, &s_ctx);
    khc_set_cb_sock_close(&http, khct::cb::mock_close, &s_ctx);

    khct::cb::IOCtx io_ctx;
    khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
    khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);

    Result result = {};

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
        if (next == peer.reads.size()) {
            if (peer.closes) {
                return EBISU_SOCK_OK;
            }
            ++result.timeouts;
            return EBISU_SOCK_FAIL;
        }
        *out_actual_length = peer.reads[next].copy(buffer, length_to_read, offset);
        offset += *out_actual_length;
        if (offset == peer.reads[next].size()) {
            ++next;
            offset = 0;
        }
        return EBISU_SOCK_OK;
    };
    s_ctx.on_close = [&result](void*) {
        ++result.on_close_called;
        return EBISU_SOCK_OK;
    };
    io_ctx.on_header = [](char*, size_t size, void*) {
        return size;
    };
    io_ctx.on_write = [&result](char* buffer, size_t size, void*) {
        result.body.append(buffer, size);
        return size;
    };

    result.res = khc_perform(&http);
    return result;
}

Peer peer_with(const std::vector<std::string>& reads) {
    Peer peer;
    peer.reads = reads;
    return peer;
}

// Every way of splitting response in two reads, and one read per byte.
std::vector<std::vector<std::string>> splits_of(const std::string& response) {
    std::vector<std::vector<std::string>> splits;
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

void check_ends_without_waiting(const Result& r, const std::string& body) {
    CHECK( r.res == KHC_ERR_OK );
    CHECK( r.body == body );
    CHECK( r.timeouts == 0 );
    CHECK( r.on_close_called == 1 );
}

}

// khc used to read on until the server closed. One that was slow to, or a proxy
// holding the connection, turned a whole response into a timeout.
TEST_CASE( "A body ends at its Content-Length" ) {
    const std::string headers = STATUS + "Content-Length: 4\r\n\r\n";

    SECTION("read along with the headers") {
        check_ends_without_waiting(perform(peer_with({ headers + "body" })), "body");
    }
    SECTION("read after the headers") {
        check_ends_without_waiting(perform(peer_with({ headers, "body" })), "body");
    }
    SECTION("read in parts") {
        check_ends_without_waiting(perform(peer_with({ headers + "b", "o", "dy" })), "body");
    }
    SECTION("wherever the reads end") {
        for (const auto& reads : splits_of(headers + "body")) {
            INFO("first read " << reads[0]);
            check_ends_without_waiting(perform(peer_with(reads)), "body");
        }
    }
    SECTION("of 0") {
        check_ends_without_waiting(
                perform(peer_with({ STATUS + "Content-Length: 0\r\n\r\n" })), "");
    }
}

TEST_CASE( "What follows a body's Content-Length isn't body" ) {
    const std::string headers = STATUS + "Content-Length: 4\r\n\r\n";

    SECTION("read along with the headers") {
        check_ends_without_waiting(perform(peer_with({ headers + "bodyEXTRA" })), "body");
    }
    SECTION("read along with the body") {
        check_ends_without_waiting(perform(peer_with({ headers, "bodyEXTRA" })), "body");
    }
    SECTION("read along with the end of the body") {
        check_ends_without_waiting(perform(peer_with({ headers + "bo", "dyEXTRA" })), "body");
    }
    SECTION("when it is 0") {
        check_ends_without_waiting(
                perform(peer_with({ STATUS + "Content-Length: 0\r\n\r\nEXTRA" })), "");
    }
}

TEST_CASE( "A body short of its Content-Length still fails" ) {
    const std::string headers = STATUS + "Content-Length: 5\r\n\r\n";

    SECTION("on a read that times out") {
        Result r = perform(peer_with({ headers, "body" }));
        CHECK( r.res == KHC_ERR_SOCK_RECV );
        CHECK( r.body == "body" );
        CHECK( r.timeouts == 1 );
    }
    SECTION("on the connection closing") {
        Peer peer = peer_with({ headers, "body" });
        peer.closes = true;
        Result r = perform(peer);
        CHECK( r.res == KHC_ERR_SOCK_RECV );
        CHECK( r.body == "body" );
        CHECK( r.timeouts == 0 );
    }
}

// With neither a length nor chunks, nothing else says where the body ends.
TEST_CASE( "A body of no stated length ends with the connection" ) {
    Peer peer = peer_with({ STATUS + "\r\n" + "body" });

    SECTION("closing") {
        peer.closes = true;
        check_ends_without_waiting(perform(peer), "body");
    }
    SECTION("timing out fails") {
        Result r = perform(peer);
        CHECK( r.res == KHC_ERR_SOCK_RECV );
        CHECK( r.body == "body" );
        CHECK( r.timeouts == 1 );
    }
}

TEST_CASE( "A chunked body ends after its last chunk" ) {
    const std::string bodies[] = {
        "4\r\nbody\r\n0\r\n\r\n",
        "2\r\nbo\r\n2\r\ndy\r\n0\r\n\r\n",
        "4;a=b\r\nbody\r\n0;a=b\r\n\r\n",
        // Trailers.
        "4\r\nbody\r\n0\r\nX-A: 1\r\n\r\n",
        "4\r\nbody\r\n0\r\nX-A: 1\r\nX-B: 2\r\n\r\n",
        // Trailers that only look like the end.
        "4\r\nbody\r\n0\r\nX-A: \r1\r\n\r\n",
        "4\r\nbody\r\n0\r\nX-A: 1\r\r\n\r\n",
        "4\r\nbody\r\n0\r\nX-A: 1\n\n\r\n\r\n",
    };

    for (const auto& body : bodies) {
        for (const auto& reads : splits_of(CHUNKED + body)) {
            INFO("body " << body << ", first read " << reads[0]);
            check_ends_without_waiting(perform(peer_with(reads)), "body");
        }
    }
}

// So that what was read along with the headers is still in their buffer when
// the last chunk has been parsed.
TEST_CASE( "A chunked body read through a small stream buffer ends too" ) {
    for (const std::string& trailers : { std::string(""), std::string("X-A: 1\r\n") }) {
        INFO("trailers " << trailers);
        Peer peer = peer_with({ CHUNKED + "4\r\nbody\r\n0\r\n" + trailers + "\r\n" });
        peer.stream_buff_size = 8;
        check_ends_without_waiting(perform(peer), "body");
    }
}

TEST_CASE( "What follows the last chunk isn't body" ) {
    check_ends_without_waiting(
            perform(peer_with({ CHUNKED + "4\r\nbody\r\n0\r\n\r\nEXTRA" })), "body");
    check_ends_without_waiting(
            perform(peer_with({ CHUNKED, "4\r\nbody\r\n0\r\n\r\n4\r\nmore\r\n0\r\n\r\n" })), "body");
}

// The whole body has come, and only trailers that nobody reads were cut off.
TEST_CASE( "A connection closed after the last chunk is a response" ) {
    const std::string bodies[] = {
        "4\r\nbody\r\n0\r\n",
        "4\r\nbody\r\n0\r\n\r",
        "4\r\nbody\r\n0\r\nX-A: 1\r\n",
    };
    for (const auto& body : bodies) {
        INFO("body " << body);
        Peer peer = peer_with({ CHUNKED + body });
        peer.closes = true;
        check_ends_without_waiting(perform(peer), "body");
    }
}

TEST_CASE( "A last chunk whose end doesn't come times out" ) {
    Result r = perform(peer_with({ CHUNKED + "4\r\nbody\r\n0\r\n" }));
    CHECK( r.res == KHC_ERR_SOCK_RECV );
    CHECK( r.body == "body" );
    CHECK( r.timeouts == 1 );
}

TEST_CASE( "Chunks decide where a body ends, not Content-Length" ) {
    const std::string body = "\r\n\r\n4\r\nbody\r\n0\r\n\r\n";
    const std::string chunked = "Transfer-Encoding: chunked";

    check_ends_without_waiting(
            perform(peer_with({ STATUS + chunked + "\r\nContent-Length: 1" + body })), "body");
    check_ends_without_waiting(
            perform(peer_with({ STATUS + "Content-Length: 1\r\n" + chunked + body })), "body");
}

TEST_CASE( "A response with no body ends with its headers" ) {
    Peer peer;
    SECTION("HEAD with a Content-Length") {
        peer = peer_with({ STATUS + "Content-Length: 100\r\n\r\n" });
        peer.method = "HEAD";
    }
    SECTION("HEAD with chunks") {
        peer = peer_with({ STATUS + "Transfer-Encoding: chunked\r\n\r\n" });
        peer.method = "HEAD";
    }
    SECTION("HEAD with neither") {
        peer = peer_with({ STATUS + "\r\n" });
        peer.method = "HEAD";
    }
    SECTION("204") {
        peer = peer_with({ "HTTP/1.1 204 No Content\r\n\r\n" });
    }
    SECTION("204 followed by something") {
        peer = peer_with({ "HTTP/1.1 204 No Content\r\n\r\nEXTRA" });
    }
    SECTION("304 with a Content-Length") {
        peer = peer_with({ "HTTP/1.1 304 Not Modified\r\nContent-Length: 100\r\n\r\n" });
    }
    check_ends_without_waiting(perform(peer), "");
}

TEST_CASE( "Several Content-Length headers" ) {
    SECTION("that agree") {
        check_ends_without_waiting(perform(peer_with({
            STATUS + "Content-Length: 4\r\nContent-Length: 4\r\n\r\nbody" })), "body");
    }
    SECTION("that disagree") {
        Result r = perform(peer_with({
            STATUS + "Content-Length: 4\r\nContent-Length: 3\r\n\r\nbody" }));
        CHECK( r.res == KHC_ERR_HEADER_CALLBACK );
        CHECK( r.body == "" );
        CHECK( r.on_close_called == 1 );
    }
}

TEST_CASE( "An interim response's Content-Length doesn't end the final body" ) {
    Peer peer = peer_with({
        "HTTP/1.1 100 Continue\r\nContent-Length: 0\r\n\r\n" + STATUS + "\r\nbody" });
    peer.closes = true;
    check_ends_without_waiting(perform(peer), "body");
}
