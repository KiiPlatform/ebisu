#include "catch.hpp"
#include "khc.h"
#include "http_script.h"

#include <string>
#include <string.h>

TEST_CASE( "khc_reset_except_request sends the same request again" ) {
    khc http;
    khc_init(&http);
    khct::HttpScript script;
    script.attach(&http);

    const std::string body = "{\"k\":1}";
    size_t body_pos = 0;
    std::string received;
    khct::cb::IOCtx io;
    io.on_read = [&](char* buffer, size_t size, void*) {
        size_t n = body.size() - body_pos;
        if (n > size) {
            n = size;
        }
        memcpy(buffer, body.data() + body_pos, n);
        body_pos += n;
        return n;
    };
    io.on_write = [&](char* buffer, size_t size, void*) {
        received.append(buffer, size);
        return size;
    };
    io.on_header = [](char*, size_t size, void*) { return size; };
    khc_set_cb_read(&http, khct::cb::cb_read, &io);
    khc_set_cb_write(&http, khct::cb::cb_write, &io);
    khc_set_cb_header(&http, khct::cb::cb_header, &io);

    khc_set_host(&http, "api.kii.com");
    khc_set_method(&http, "POST");
    khc_set_path(&http, "/x");
    khc_slist* headers = khc_slist_append(NULL, "X-Test: 1", strlen("X-Test: 1"));
    khc_set_req_headers(&http, headers);

    // The first response is chunked and the second is not: state left over from
    // reading the first would misread the second.
    script.responses = {
        "HTTP/1.1 401 Unauthorized\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "7\r\n{\"e\":1}\r\n0\r\n\r\n",
        khct::http_response(200, "{\"ok\":true}")
    };

    REQUIRE( khc_perform(&http) == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 401 );
    REQUIRE( received == "{\"e\":1}" );

    khc_reset_except_request(&http);
    REQUIRE( khc_get_status_code(&http) == 0 );
    body_pos = 0;
    received.clear();

    REQUIRE( khc_perform(&http) == KHC_ERR_OK );
    REQUIRE( khc_get_status_code(&http) == 200 );
    REQUIRE( received == "{\"ok\":true}" );

    REQUIRE( script.requests.size() == 2 );
    REQUIRE( script.requests[0] == script.requests[1] );
    REQUIRE( khct::contains(script.requests[1], "POST https://api.kii.com/x HTTP/1.1\r\n") );
    REQUIRE( khct::contains(script.requests[1], "X-Test: 1\r\n") );
    REQUIRE( khct::contains(script.requests[1], body) );

    khc_slist_free_all(headers);
}

namespace {

// The request every khc in the next test is given, and what it reads the body
// from and writes the response to.
struct Request {
    khct::HttpScript script;
    khct::cb::IOCtx io;
    khc_slist* headers;
    char stream_buff[64];
    char resp_header_buff[256];

    Request() {
        io.on_read = [](char*, size_t, void*) { return (size_t)0; };
        io.on_write = [](char*, size_t size, void*) { return size; };
        io.on_header = [](char*, size_t size, void*) { return size; };
        headers = khc_slist_append(NULL, "X-Test: 1", strlen("X-Test: 1"));
    }

    ~Request() {
        khc_slist_free_all(headers);
    }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    // Everything a request consists of: what kii sets for each one, and the
    // callbacks and settings it keeps across them.
    void set(khc* http, bool own_buffers) {
        script.attach(http);
        khc_set_cb_read(http, khct::cb::cb_read, &io);
        khc_set_cb_write(http, khct::cb::cb_write, &io);
        khc_set_cb_header(http, khct::cb::cb_header, &io);
        khc_enable_insecure(http, false);
        khc_set_host(http, "api.kii.com");
        khc_set_method(http, "PUT");
        khc_set_path(http, "/x");
        khc_set_req_headers(http, headers);
        if (own_buffers) {
            khc_set_stream_buff(http, stream_buff, sizeof(stream_buff));
            khc_set_resp_header_buff(http, resp_header_buff, sizeof(resp_header_buff));
        } else {
            khc_set_stream_buff(http, NULL, 0);
            khc_set_resp_header_buff(http, NULL, 0);
        }
    }

    // Filled before khc_init(), which leaves the padding alone, so that two
    // set up alike compare equal byte for byte.
    void init(khc* http, bool own_buffers, unsigned char fill) {
        memset(http, fill, sizeof(*http));
        khc_init(http);
        set(http, own_buffers);
    }
};

}

// Every member of struct khc either belongs to the request, which
// khc_reset_except_request() keeps, or to the exchange, which it puts back as
// khc_init() leaves it. The two are compared whole, so that a member added
// later and left out of khc_reset_except_request() fails here rather than
// misreading the response to a request sent again.
TEST_CASE( "khc_reset_except_request leaves khc as if the request were new" ) {
    const std::string responses[] = {
        "HTTP/1.1 401 Unauthorized\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "7\r\n{\"e\":1}\r\n0\r\n\r\n",
        khct::http_response(401, "{\"e\":1}"),
    };
    for (const std::string& response : responses) {
        for (bool own_buffers : { true, false }) {
            INFO("own buffers " << own_buffers << ", response " << response);
            Request req;
            khc used;
            khc fresh;
            req.init(&used, own_buffers, 0);
            req.init(&fresh, own_buffers, 0);

            req.script.responses = { response };
            REQUIRE( khc_perform(&used) == KHC_ERR_OK );
            REQUIRE( khc_get_status_code(&used) == 401 );
            khc_reset_except_request(&used);

            // Named first, for a failure that says which. The char* members
            // are compared as pointers: given a char*, Catch prints the string
            // it points to, and these point into buffers that need not hold one.
            CHECK( used._state == fresh._state );
            CHECK( used._current_req_header == fresh._current_req_header );
            CHECK( used._read_size == fresh._read_size );
            CHECK( used._read_req_end == fresh._read_req_end );
            CHECK( used._resp_header_read_size == fresh._resp_header_read_size );
            CHECK( used._status_code == fresh._status_code );
            CHECK( (const void*)used._body_boundary == (const void*)fresh._body_boundary );
            CHECK( (const void*)used._cb_header_pos == (const void*)fresh._cb_header_pos );
            CHECK( used._cb_header_remaining_size == fresh._cb_header_remaining_size );
            CHECK( (const void*)used._body_fragment == (const void*)fresh._body_fragment );
            CHECK( used._body_fragment_size == fresh._body_fragment_size );
            CHECK( used._chunked_resp == fresh._chunked_resp );
            CHECK( used._chunk_size == fresh._chunk_size );
            CHECK( used._chunk_size_written == fresh._chunk_size_written );
            CHECK( used._resp_content_length == fresh._resp_content_length );
            CHECK( used._read_end == fresh._read_end );
            CHECK( used._body_read_size == fresh._body_read_size );
            CHECK( used._sent_length == fresh._sent_length );
            CHECK( used._result == fresh._result );
            CHECK( (const void*)used._stream_buff == (const void*)fresh._stream_buff );
            CHECK( (const void*)used._resp_header_buff == (const void*)fresh._resp_header_buff );
            // And whole, for a member not named here.
            REQUIRE( memcmp(&used, &fresh, sizeof(khc)) == 0 );
        }
    }
}

// The same from the other side, for a member an exchange happens to leave as
// khc_init() does. Every member of this khc starts as garbage and is given only
// the request and what khc_perform() leaves of buffers it allocated. A member
// added later that is neither part of the request nor reset by
// khc_reset_except_request() stays garbage, and fails here until it is one or
// the other.
TEST_CASE( "khc_reset_except_request resets all that the request does not set" ) {
    for (bool own_buffers : { true, false }) {
        INFO("own buffers " << own_buffers);
        Request req;
        khc used;
        khc fresh;
        req.init(&fresh, own_buffers, 0xA5);
        memset(&used, 0xA5, sizeof(used));
        req.set(&used, own_buffers);
        used._stream_buff_allocated = false;
        used._resp_header_buff_allocated = false;

        khc_reset_except_request(&used);
        REQUIRE( memcmp(&used, &fresh, sizeof(khc)) == 0 );
    }
}
