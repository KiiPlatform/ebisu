#include "catch.hpp"
#include <khc.h>
#include "test_callbacks.h"
#include <string>
#include <vector>

// khc_perform() called a socket operation that returned EBISU_SOCK_AGAIN again
// at once, over and over, and nothing bounded a request as a whole: a server
// sending a byte just within each receive's timeout kept it going for ever.
// With a system, khc waits between those calls, and times the request; a
// continue callback can cancel it.

namespace {

struct Fixture {
    khc http;
    khct::cb::SockCtx s_ctx;
    khct::cb::IOCtx io_ctx;
    khct::cb::SystemCtx sys;
    // The system's clock, which the socket's operations move on.
    uint64_t now_ms = 1000;
    std::vector<uint32_t> delays;
    int connects = 0;
    int closes = 0;
    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}";
    size_t served = 0;
    std::string body;

    Fixture() {
        khc_init(&http);
        khc_set_host(&http, "api.kii.com");
        khc_set_method(&http, "GET");
        khc_set_path(&http, "/api/apps");
        khc_set_req_headers(&http, NULL);
        khc_set_socket(&http, s_ctx.socket());
        khc_set_cb_write(&http, khct::cb::cb_write, &io_ctx);
        khc_set_cb_header(&http, khct::cb::cb_header, &io_ctx);
        s_ctx.on_connect = [this](void*, const char*, uint16_t) {
            ++connects;
            return EBISU_SOCK_OK;
        };
        s_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };
        s_ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            *out_actual_length = response.copy(buffer, length_to_read, served);
            served += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        s_ctx.on_close = [this](void*) {
            ++closes;
            return EBISU_SOCK_OK;
        };
        io_ctx.on_header = [](char*, size_t size, void*) { return size; };
        io_ctx.on_write = [this](char* buffer, size_t size, void*) {
            body.append(buffer, size);
            return size;
        };
        sys.on_delay_ms = [this](uint32_t ms) {
            delays.push_back(ms);
            now_ms += ms;
        };
        sys.on_monotonic_ms = [this]() { return now_ms; };
    }
};

bool continue_while_positive(void* userdata)
{
    return --*(int*)userdata >= 0;
}

}

TEST_CASE( "khc waits for a socket that isn't ready" ) {
    Fixture f;
    // Not connected yet, twice, then nothing to read, three times.
    int connect_agains = 2;
    f.s_ctx.on_connect = [&](void*, const char*, uint16_t) {
        ++f.connects;
        return connect_agains-- > 0 ? EBISU_SOCK_AGAIN : EBISU_SOCK_OK;
    };
    int recv_agains = 3;
    auto serve = f.s_ctx.on_recv;
    f.s_ctx.on_recv = [&](void* ctx, char* buffer, size_t length_to_read, size_t* out_actual_length) {
        if (recv_agains-- > 0) {
            *out_actual_length = 0;
            return EBISU_SOCK_AGAIN;
        }
        return serve(ctx, buffer, length_to_read, out_actual_length);
    };

    SECTION("with the system's delay, between the calls") {
        khc_set_system(&f.http, f.sys.system());
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.delays == std::vector<uint32_t>(5, KHC_AGAIN_WAIT_MS) );
        CHECK( f.body == "{}" );
    }

    SECTION("not at all without a system, as before") {
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.delays.empty() );
        CHECK( f.body == "{}" );
    }

    SECTION("not at all with a system without a delay") {
        f.sys.ops.delay_ms = NULL;
        khc_set_system(&f.http, f.sys.system());
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.body == "{}" );
    }
}

TEST_CASE( "A request that takes longer than its timeout" ) {
    Fixture f;
    khc_set_system(&f.http, f.sys.system());
    khc_set_timeout_ms(&f.http, 1000);

    SECTION("ends within it, and is read whole") {
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.body == "{}" );
        CHECK( f.closes == 1 );
    }

    // Each receive ends in time, with one byte, 100 ms after the last.
    SECTION("trickling in is cut off, and closed") {
        f.response = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n" + std::string(100, 'x');
        f.s_ctx.on_recv = [&f](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            f.now_ms += 100;
            *out_actual_length = f.response.copy(buffer, 1, f.served);
            f.served += *out_actual_length;
            return EBISU_SOCK_OK;
        };
        const uint64_t started = f.now_ms;
        CHECK( khc_perform(&f.http) == KHC_ERR_TIMEOUT );
        CHECK( f.closes == 1 );
        CHECK( f.served < f.response.size() );
        CHECK( f.now_ms - started >= 1000 );
        CHECK( f.now_ms - started < 1200 );
        CHECK_FALSE( khc_get_close_failed(&f.http) );
    }

    SECTION("before connecting, and nothing is closed") {
        f.s_ctx.on_connect = [&f](void*, const char*, uint16_t) {
            ++f.connects;
            return EBISU_SOCK_AGAIN;
        };
        CHECK( khc_perform(&f.http) == KHC_ERR_TIMEOUT );
        CHECK( f.connects == 1000 / KHC_AGAIN_WAIT_MS );
        CHECK( f.closes == 0 );
    }

    SECTION("with a close that isn't ready, gives the close up") {
        f.s_ctx.on_recv = [](void*, char*, size_t, size_t* out_actual_length) {
            *out_actual_length = 0;
            return EBISU_SOCK_AGAIN;
        };
        f.s_ctx.on_close = [&f](void*) {
            ++f.closes;
            return EBISU_SOCK_AGAIN;
        };
        CHECK( khc_perform(&f.http) == KHC_ERR_TIMEOUT );
        CHECK( f.closes == 1 );
        CHECK( khc_get_close_failed(&f.http) );
    }

    SECTION("needs a clock, or nothing is sent") {
        f.sys.ops.monotonic_ms = NULL;
        CHECK( khc_perform(&f.http) == KHC_ERR_FAIL );
        CHECK( f.connects == 0 );

        khc_set_system(&f.http, NULL);
        CHECK( khc_perform(&f.http) == KHC_ERR_FAIL );
        CHECK( f.connects == 0 );
    }

    SECTION("of 0 is none") {
        khc_set_timeout_ms(&f.http, 0);
        khc_set_system(&f.http, NULL);
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
    }

    // The next request is timed from its own start.
    SECTION("is timed from its start, each time") {
        f.now_ms += 5000;
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        f.now_ms += 5000;
        khc_reset_except_request(&f.http);
        f.served = 0;
        f.body.clear();
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.body == "{}" );
    }
}

TEST_CASE( "A request the continue callback cancels" ) {
    Fixture f;
    int steps_left = 0;
    khc_set_cb_continue(&f.http, continue_while_positive, &steps_left);

    SECTION("before it starts connects to nothing") {
        steps_left = 0;
        CHECK( khc_perform(&f.http) == KHC_ERR_CANCELLED );
        CHECK( f.connects == 0 );
        CHECK( f.closes == 0 );
    }

    SECTION("once connected is closed") {
        // Idle, connect, then cancelled at the request line.
        steps_left = 2;
        CHECK( khc_perform(&f.http) == KHC_ERR_CANCELLED );
        CHECK( f.connects == 1 );
        CHECK( f.closes == 1 );
        CHECK( f.served == 0 );
    }

    SECTION("that goes on is read whole, the callback asked at each step") {
        steps_left = 1000;
        CHECK( khc_perform(&f.http) == KHC_ERR_OK );
        CHECK( f.body == "{}" );
        CHECK( steps_left < 1000 - 5 );
        CHECK( steps_left >= 0 );
    }
}

TEST_CASE( "The resets keep the system, the timeout and the continue callback" ) {
    Fixture f;
    int steps_left = 1000;
    khc_set_system(&f.http, f.sys.system());
    khc_set_timeout_ms(&f.http, 1000);
    khc_set_cb_continue(&f.http, continue_while_positive, &steps_left);

    SECTION("khc_reset_except_request()") {
        khc_reset_except_request(&f.http);
    }
    SECTION("khc_reset_except_cb()") {
        khc_reset_except_cb(&f.http);
    }
    CHECK( f.http._system == f.sys.system() );
    CHECK( f.http._timeout_ms == 1000 );
    CHECK( f.http._cb_continue == continue_while_positive );
    CHECK( f.http._continue_data == &steps_left );
}
