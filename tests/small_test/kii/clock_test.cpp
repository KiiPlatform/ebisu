#include "catch.hpp"
#include "kii.h"
#include "khc.h"
#include "ebisu_version.h"
#include "test_callbacks.h"

#include <string>
#include <string.h>
#include <vector>

namespace {

// 2090-01-01T00:00:00Z and 2090-06-15T12:00:00Z: later than any source this
// is built from will be, so no floor gets in their way.
const int64_t JAN_2090 = 3786912000LL;
const int64_t JUN_2090 = 3801211200LL;
const char JAN_2090_DATE[] = "Date: Sun, 01 Jan 2090 00:00:00 GMT";
const char JUN_2090_DATE[] = "Date: Thu, 15 Jun 2090 12:00:00 GMT";

// The monotonic clock and lock the tests hand the clock, and what it did with
// them.
struct Platform {
    uint64_t now_ms;
    int depth;
    int max_depth;
    int locks;
    int clock_reads_unlocked;
    bool locked;
    Platform() : now_ms(1000000), depth(0), max_depth(0), locks(0),
        clock_reads_unlocked(0), locked(false) {}
};

uint64_t now_ms(void* userdata) {
    Platform* p = (Platform*)userdata;
    if (p->locked && p->depth == 0) {
        ++p->clock_reads_unlocked;
    }
    return p->now_ms;
}

void lock(void* userdata) {
    Platform* p = (Platform*)userdata;
    ++p->locks;
    if (++p->depth > p->max_depth) {
        p->max_depth = p->depth;
    }
}

void unlock(void* userdata) {
    --((Platform*)userdata)->depth;
}

int64_t time_of(kii_clock_t* clock, bool* exact) {
    int64_t now = 0;
    *exact = kii_clock_get_time(clock, &now) == KII_TRUE;
    return now;
}

} // namespace

TEST_CASE( "A new clock has the floor khc has, and no time" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    khc http;
    khc_init(&http);
    CHECK( clock._floor == http._resp_date_floor );
    CHECK( clock._floor == EBISU_SOURCE_TIME - 7 * 86400 );

    bool exact = true;
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );
}

TEST_CASE( "The floor is only ever raised" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    const int64_t floor = clock._floor;
    bool exact = true;

    kii_clock_set_floor(&clock, floor - 1);
    CHECK( time_of(&clock, &exact) == floor );
    kii_clock_set_floor(&clock, JAN_2090);
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK_FALSE( exact );
    kii_clock_set_floor(&clock, JAN_2090 - 100);
    CHECK( time_of(&clock, &exact) == JAN_2090 );
}

TEST_CASE( "A time given is counted on with the monotonic clock" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    bool exact = false;

    // Earlier than the floor: not taken.
    CHECK( kii_clock_set_time(&clock, clock._floor - 1) == KII_FALSE );
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );

    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK( exact );
    // Whole seconds elapsed.
    p.now_ms += 999;
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 1 );
    p.now_ms += 3600 * 1000ULL;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 3601 );
    CHECK( exact );

    // A monotonic clock going back cannot count on from the time given, which
    // becomes the floor: the time is at least that.
    p.now_ms = 0;
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK_FALSE( exact );
    CHECK( clock._floor == JAN_2090 );
    p.now_ms = 5000;
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK_FALSE( exact );

    // A floor set later than the time counted on is the better bound, but a
    // bound only.
    kii_clock_set_floor(&clock, JUN_2090);
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK_FALSE( exact );
    // And a time given after it, later still, is the time again.
    REQUIRE( kii_clock_set_time(&clock, JUN_2090 + 10) == KII_TRUE );
    CHECK( time_of(&clock, &exact) == JUN_2090 + 10 );
    CHECK( exact );
}

TEST_CASE( "A time given stays exact for a day, and is a lower bound after" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    bool exact = false;
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );

    p.now_ms += 24ULL * 3600 * 1000 - 1;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 86399 );
    CHECK( exact );
    // A day after: counted on, less 1%, and a lower bound only.
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 86400 - 864 );
    CHECK_FALSE( exact );
    p.now_ms += 99ULL * 86400 * 1000;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 100 * 86400 - 86400 );
    CHECK_FALSE( exact );

    // A response, or the application, giving the time again makes it exact.
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK( exact );

    // A lower floor than the stale count is no better bound; a higher one is.
    kii_clock_init(&clock, now_ms, &p);
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    p.now_ms += 10ULL * 86400 * 1000;
    kii_clock_set_floor(&clock, JAN_2090 + 10 * 86400);
    CHECK( time_of(&clock, &exact) == JAN_2090 + 10 * 86400 );
    CHECK_FALSE( exact );
}

TEST_CASE( "How long a time stays exact can be set, to never as well" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    bool exact = false;

    kii_clock_set_exact_for(&clock, 60);
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    p.now_ms += 59999;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 59 );
    CHECK( exact );
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 60 );
    CHECK_FALSE( exact );

    kii_clock_set_exact_for(&clock, 0);
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK_FALSE( exact );
}

TEST_CASE( "A clock long without a time does not tell a period that has not started" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    p.now_ms += 30ULL * 86400 * 1000;
    // 30 days on: at least JAN_2090 + 29.7 days. A certificate issued since
    // cannot be told from one valid now, but one that ended before is expired.
    CHECK( kii_clock_check_validity(&clock, JAN_2090 + 30 * 86400, JUN_2090) == KII_VALIDITY_OK );
    CHECK( kii_clock_check_validity(&clock, JAN_2090, JAN_2090 + 29 * 86400) == KII_VALIDITY_EXPIRED );
}

TEST_CASE( "Forgetting the time keeps what was counted, less 1%, as the floor" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    bool exact = true;

    // Without a time, nothing to forget.
    const int64_t floor = clock._floor;
    kii_clock_forget_time(&clock);
    CHECK( time_of(&clock, &exact) == floor );
    CHECK_FALSE( exact );

    // A fresh time: counted, less 1%.
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    p.now_ms += 3600 * 1000ULL;
    kii_clock_forget_time(&clock);
    CHECK( time_of(&clock, &exact) == JAN_2090 + 3600 - 36 );
    CHECK_FALSE( exact );
    // And it stays forgotten as the monotonic clock goes on.
    p.now_ms += 3600 * 1000ULL;
    CHECK( time_of(&clock, &exact) == JAN_2090 + 3600 - 36 );
    CHECK_FALSE( exact );

    // A stale time: counted less 1% already.
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    p.now_ms += 2ULL * 86400 * 1000;
    kii_clock_forget_time(&clock);
    CHECK( time_of(&clock, &exact) == JUN_2090 + 2 * 86400 - 1728 );
    CHECK_FALSE( exact );

    // After the monotonic clock went back, the time given.
    kii_clock_init(&clock, now_ms, &p);
    REQUIRE( kii_clock_set_time(&clock, JAN_2090) == KII_TRUE );
    p.now_ms -= 1000;
    kii_clock_forget_time(&clock);
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK_FALSE( exact );

    // A time given again is the time again.
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK( exact );
}

TEST_CASE( "A validity period is checked against the time, or against its floor" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    kii_clock_set_floor(&clock, JAN_2090);

    // With the floor only: expired if it ended before it, valid otherwise,
    // even if it starts later, which could not be told from now.
    CHECK( kii_clock_check_validity(&clock, 0, JAN_2090 - 1) == KII_VALIDITY_EXPIRED );
    CHECK( kii_clock_check_validity(&clock, 0, JAN_2090) == KII_VALIDITY_OK );
    CHECK( kii_clock_check_validity(&clock, JUN_2090, JUN_2090 + 86400) == KII_VALIDITY_OK );

    // With the time: either end.
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    CHECK( kii_clock_check_validity(&clock, JAN_2090, JUN_2090 - 1) == KII_VALIDITY_EXPIRED );
    CHECK( kii_clock_check_validity(&clock, JUN_2090 + 1, JUN_2090 + 86400) == KII_VALIDITY_NOT_YET_VALID );
    // Both ends inclusive.
    CHECK( kii_clock_check_validity(&clock, JUN_2090, JUN_2090) == KII_VALIDITY_OK );
    CHECK( kii_clock_check_validity(&clock, JAN_2090, JUN_2090 + 86400) == KII_VALIDITY_OK );
}

TEST_CASE( "With a lock set, the clock is used and read only with it held" ) {
    Platform p;
    p.locked = true;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    kii_clock_set_cb_lock(&clock, lock, unlock, &p);

    int64_t now = 0;
    kii_clock_set_floor(&clock, JAN_2090);
    REQUIRE( kii_clock_set_time(&clock, JUN_2090) == KII_TRUE );
    kii_clock_get_time(&clock, &now);
    kii_clock_check_validity(&clock, 0, JUN_2090);

    CHECK( p.locks == 4 );
    CHECK( p.depth == 0 );
    CHECK( p.max_depth == 1 );
    CHECK( p.clock_reads_unlocked == 0 );
}

TEST_CASE( "kii_set_clock() needs a monotonic clock" ) {
    kii_t kii;
    kii_init(&kii);
    kii_clock_t clock;
    kii_clock_init(&clock, NULL, NULL);
    CHECK( kii_set_clock(&kii, &clock) == KII_ERR_FAIL );
    CHECK( kii._clock == nullptr );

    Platform p;
    kii_clock_init(&clock, now_ms, &p);
    CHECK( kii_set_clock(&kii, &clock) == KII_ERR_OK );
    CHECK( kii._clock == &clock );
    CHECK( kii_set_clock(&kii, NULL) == KII_ERR_OK );
    CHECK( kii._clock == nullptr );
}

namespace {

// A kii_t whose requests are each answered, on a connection of their own, by
// the next of responses. It notes the time its clock has as each connection
// opens, as a socket callback checking the server's certificate would.
struct Client {
    char buff[1024];
    jkii_token_t tokens[64];
    jkii_resource_t resource;
    kii_t kii;
    kii_clock_t* clock;
    std::vector<std::string> responses;
    size_t next;
    std::string resp;
    size_t resp_pos;
    std::vector<std::pair<int64_t, bool> > times_at_connect;
    khct::cb::SockCtx http_ctx;

    explicit Client(kii_clock_t* c) : clock(c), next(0), resp_pos(0) {
        resource.tokens = tokens;
        resource.tokens_num = 64;
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "dummyAppID");
        kii_set_buff(&kii, buff, sizeof(buff));
        kii_set_json_parser_resource(&kii, &resource);
        http_ctx.on_connect = [this](void*, const char*, unsigned int) {
            int64_t now = 0;
            bool exact = kii_clock_get_time(clock, &now) == KII_TRUE;
            times_at_connect.push_back(std::make_pair(now, exact));
            resp = (next < responses.size()) ? responses[next] : std::string();
            ++next;
            resp_pos = 0;
            return KHC_SOCK_OK;
        };
        http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return KHC_SOCK_OK;
        };
        http_ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            size_t n = resp.size() - resp_pos;
            if (n > length_to_read) { n = length_to_read; }
            memcpy(buffer, resp.data() + resp_pos, n);
            resp_pos += n;
            *out_actual_length = n;
            return KHC_SOCK_OK;
        };
        http_ctx.on_close = [](void*) { return KHC_SOCK_OK; };
        kii_set_cb_http_sock_connect(&kii, khct::cb::mock_connect, &http_ctx);
        kii_set_cb_http_sock_send(&kii, khct::cb::mock_send, &http_ctx);
        kii_set_cb_http_sock_recv(&kii, khct::cb::mock_recv, &http_ctx);
        kii_set_cb_http_sock_close(&kii, khct::cb::mock_close, &http_ctx);
    }

    kii_code_t get() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, KII_FALSE);
        return (res == KII_ERR_OK) ? kii_api_call_run(&kii) : res;
    }
};

std::string response(const std::string& date) {
    return "HTTP/1.1 200 OK\r\n" + (date.empty() ? std::string() : date + "\r\n") +
        "Content-Length: 2\r\n\r\n{}";
}

} // namespace

TEST_CASE( "The first connection has the floor, and the next the time the server stated" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    Client c(&clock);
    REQUIRE( kii_set_clock(&c.kii, &clock) == KII_ERR_OK );
    c.responses = { response(JAN_2090_DATE), response(""), response(JUN_2090_DATE) };

    REQUIRE( c.get() == KII_ERR_OK );
    p.now_ms += 5000;
    // No Date in this one: the clock counts on from the last.
    REQUIRE( c.get() == KII_ERR_OK );
    p.now_ms += 1000;
    REQUIRE( c.get() == KII_ERR_OK );

    REQUIRE( c.times_at_connect.size() == 3 );
    CHECK( c.times_at_connect[0].first == clock._floor );
    CHECK_FALSE( c.times_at_connect[0].second );
    CHECK( c.times_at_connect[1].first == JAN_2090 + 5 );
    CHECK( c.times_at_connect[1].second );
    CHECK( c.times_at_connect[2].first == JAN_2090 + 6 );
    CHECK( c.times_at_connect[2].second );

    bool exact = false;
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK( exact );
}

TEST_CASE( "Every kii_t sharing a clock has the time one of them got" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    Client first(&clock);
    Client second(&clock);
    REQUIRE( kii_set_clock(&first.kii, &clock) == KII_ERR_OK );
    REQUIRE( kii_set_clock(&second.kii, &clock) == KII_ERR_OK );
    first.responses = { response(JAN_2090_DATE) };
    second.responses = { response("") };

    REQUIRE( first.get() == KII_ERR_OK );
    REQUIRE( second.get() == KII_ERR_OK );
    REQUIRE( second.times_at_connect.size() == 1 );
    CHECK( second.times_at_connect[0].first == JAN_2090 );
    CHECK( second.times_at_connect[0].second );
}

TEST_CASE( "A date earlier than the clock's floor does not set it" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    // Saved by the device: later than the server says, which khc's own floor
    // cannot know.
    kii_clock_set_floor(&clock, JUN_2090);
    Client c(&clock);
    REQUIRE( kii_set_clock(&c.kii, &clock) == KII_ERR_OK );
    c.responses = { response(JAN_2090_DATE) };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == JAN_2090 );

    bool exact = true;
    CHECK( time_of(&clock, &exact) == JUN_2090 );
    CHECK_FALSE( exact );
}

TEST_CASE( "Without a clock set, responses set none" ) {
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    Client c(&clock);
    c.responses = { response(JAN_2090_DATE) };
    REQUIRE( c.get() == KII_ERR_OK );
    bool exact = true;
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );
}

TEST_CASE( "A certificate renewed while the clock fell behind is refused once, then taken" ) {
    // The device's monotonic clock stopped while it slept for 10 days, during
    // which the server renewed its certificate: valid from JAN_2090 + 10 days.
    // A socket callback refusing it as not valid yet forgets the time, so the
    // next connection is checked against a lower bound and goes through, and
    // its response gives the clock the time again.
    Platform p;
    kii_clock_t clock;
    kii_clock_init(&clock, now_ms, &p);
    Client c(&clock);
    REQUIRE( kii_set_clock(&c.kii, &clock) == KII_ERR_OK );
    int64_t not_before = 0;
    const int64_t not_after = JUN_2090 + 365 * 86400;
    int refused = 0;
    auto check_certificate = c.http_ctx.on_connect;
    c.http_ctx.on_connect = [&](void* ctx, const char* host, unsigned int port) {
        if (kii_clock_check_validity(&clock, not_before, not_after) != KII_VALIDITY_OK) {
            kii_clock_forget_time(&clock);
            ++refused;
            return KHC_SOCK_FAIL;
        }
        return check_certificate(ctx, host, port);
    };
    c.responses = {
        response(JAN_2090_DATE),
        response("Date: Wed, 11 Jan 2090 00:00:00 GMT"),
        response(""),
    };

    // Before the sleep: the old certificate, and the time from the response.
    not_before = JAN_2090 - 86400;
    REQUIRE( c.get() == KII_ERR_OK );
    bool exact = false;
    REQUIRE( time_of(&clock, &exact) == JAN_2090 );
    REQUIRE( exact );

    // After it: the clock says JAN_2090 still, the certificate is newer.
    not_before = JAN_2090 + 10 * 86400;
    CHECK( c.get() != KII_ERR_OK );
    CHECK( refused == 1 );
    CHECK( time_of(&clock, &exact) == JAN_2090 );
    CHECK_FALSE( exact );

    // The next connection goes through, and its response sets the time.
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( refused == 1 );
    CHECK( time_of(&clock, &exact) == JAN_2090 + 10 * 86400 );
    CHECK( exact );

    // From then on, the renewed certificate is valid at the time.
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( refused == 1 );
}
