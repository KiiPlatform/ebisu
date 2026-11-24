#include "catch_amalgamated.hpp"
#include "kii.h"
#include "khc.h"
#include "ebisu_server_clock.h"
#include "ebisu_version.h"
#include "ebisu_time.h"
#include "test_callbacks.h"

#include <cstdio>
#include <string>
#include <string.h>
#include <vector>

namespace {

// A Date header stating seconds since the epoch, in IMF-fixdate.
std::string date_header(int64_t seconds) {
    static const char* const days[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* const months[12] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    ebisu_civil_time_t t;
    REQUIRE( ebisu_time_to_civil(seconds, &t) );
    char s[64];
    snprintf(s, sizeof(s), "Date: %s, %02d %s %04d %02d:%02d:%02d GMT",
            days[(seconds / 86400 + 4) % 7], t.day, months[t.month - 1], t.year,
            t.hour, t.minute, t.second);
    return s;
}

// Midnight a month after the source this is built from, and 165 and a half
// days after that: past the floors, and within the year that is the least
// EBISU_DATE_CEILING_YEARS can be, but none.
const int64_t SOON = (EBISU_SOURCE_TIME / 86400 + 30) * 86400;
const int64_t LATER = SOON + 165 * 86400 + 12 * 3600;

uint64_t now_ms(void* userdata);
void lock(void* userdata);
void unlock(void* userdata);

// The monotonic clock and lock the tests hand the clock, and what it did with
// them.
struct Platform {
    uint64_t now_ms;
    int depth;
    int max_depth;
    int locks;
    int clock_reads_unlocked;
    bool locked;
    khct::cb::SystemCtx system;
    khct::cb::LockCtx lock;
    Platform() : now_ms(1000000), depth(0), max_depth(0), locks(0),
        clock_reads_unlocked(0), locked(false) {
        system.on_monotonic_ms = [this] { return ::now_ms(this); };
        lock.on_lock = [this] { ::lock(this); };
        lock.on_unlock = [this] { ::unlock(this); };
    }
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

int64_t time_of(ebisu_server_clock_t* clock, bool* exact) {
    int64_t now = 0;
    *exact = ebisu_clock_get_time(&clock->base, &now);
    return now;
}

} // namespace

TEST_CASE( "A new clock has the floor khc has, and no time" ) {
    Platform p;
    ebisu_server_clock_t clock;
    REQUIRE( ebisu_server_clock_init(&clock, p.system.system()) );
    khc_t http;
    khc_init(&http);
    CHECK( clock._floor == http._resp_date_floor );
    CHECK( clock._floor == EBISU_SOURCE_TIME - 7 * 86400 );
    // The floor a system's time of day earlier than it gives way to.
    int64_t system_floor = 0;
    CHECK_FALSE( ebisu_clock_from_system_time(0, &system_floor) );
    CHECK( system_floor == clock._floor );

    bool exact = true;
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );
}

TEST_CASE( "The floor is only ever raised" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    const int64_t floor = clock._floor;
    bool exact = true;

    ebisu_server_clock_set_floor(&clock, floor - 1);
    CHECK( time_of(&clock, &exact) == floor );
    ebisu_server_clock_set_floor(&clock, SOON);
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK_FALSE( exact );
    ebisu_server_clock_set_floor(&clock, SOON - 100);
    CHECK( time_of(&clock, &exact) == SOON );
}

TEST_CASE( "A time given is counted on with the monotonic clock" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    bool exact = false;

    // Earlier than the floor: not taken.
    CHECK( ebisu_server_clock_set_time(&clock, clock._floor - 1) == false );
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );

    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK( exact );
    // Whole seconds elapsed.
    p.now_ms += 999;
    CHECK( time_of(&clock, &exact) == SOON );
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == SOON + 1 );
    p.now_ms += 3600 * 1000ULL;
    CHECK( time_of(&clock, &exact) == SOON + 3601 );
    CHECK( exact );

    // A monotonic clock going back cannot count on from the time given, which
    // becomes the floor: the time is at least that.
    p.now_ms = 0;
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK_FALSE( exact );
    CHECK( clock._floor == SOON );
    p.now_ms = 5000;
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK_FALSE( exact );

    // A floor set later than the time counted on is the better bound, but a
    // bound only.
    ebisu_server_clock_set_floor(&clock, LATER);
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK_FALSE( exact );
    // And a time given after it, later still, is the time again.
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER + 10) == true );
    CHECK( time_of(&clock, &exact) == LATER + 10 );
    CHECK( exact );
}

TEST_CASE( "A time given stays exact for a day, and is a lower bound after" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    bool exact = false;
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );

    p.now_ms += 24ULL * 3600 * 1000 - 1;
    CHECK( time_of(&clock, &exact) == SOON + 86399 );
    CHECK( exact );
    // A day after: counted on, less 1%, and a lower bound only.
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == SOON + 86400 - 864 );
    CHECK_FALSE( exact );
    p.now_ms += 99ULL * 86400 * 1000;
    CHECK( time_of(&clock, &exact) == SOON + 100 * 86400 - 86400 );
    CHECK_FALSE( exact );

    // A response, or the application, giving the time again makes it exact.
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK( exact );

    // A lower floor than the stale count is no better bound; a higher one is.
    ebisu_server_clock_init(&clock, p.system.system());
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    p.now_ms += 10ULL * 86400 * 1000;
    ebisu_server_clock_set_floor(&clock, SOON + 10 * 86400);
    CHECK( time_of(&clock, &exact) == SOON + 10 * 86400 );
    CHECK_FALSE( exact );
}

TEST_CASE( "How long a time stays exact can be set, to never as well" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    bool exact = false;

    ebisu_server_clock_set_exact_for(&clock, 60);
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    p.now_ms += 59999;
    CHECK( time_of(&clock, &exact) == SOON + 59 );
    CHECK( exact );
    p.now_ms += 1;
    CHECK( time_of(&clock, &exact) == SOON + 60 );
    CHECK_FALSE( exact );

    ebisu_server_clock_set_exact_for(&clock, 0);
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK_FALSE( exact );
}

TEST_CASE( "A clock long without a time does not tell a period that has not started" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    p.now_ms += 30ULL * 86400 * 1000;
    // 30 days on: at least SOON + 29.7 days. A certificate issued since
    // cannot be told from one valid now, but one that ended before is expired.
    CHECK( ebisu_clock_check_validity(&clock.base, SOON + 30 * 86400, LATER) == EBISU_VALIDITY_OK );
    CHECK( ebisu_clock_check_validity(&clock.base, SOON, SOON + 29 * 86400) == EBISU_VALIDITY_EXPIRED );
}

TEST_CASE( "Forgetting the time keeps what was counted, less 1%, as the floor" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    bool exact = true;

    // Without a time, nothing to forget.
    const int64_t floor = clock._floor;
    ebisu_clock_forget_time(&clock.base);
    CHECK( time_of(&clock, &exact) == floor );
    CHECK_FALSE( exact );

    // A fresh time: counted, less 1%.
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    p.now_ms += 3600 * 1000ULL;
    ebisu_clock_forget_time(&clock.base);
    CHECK( time_of(&clock, &exact) == SOON + 3600 - 36 );
    CHECK_FALSE( exact );
    // And it stays forgotten as the monotonic clock goes on.
    p.now_ms += 3600 * 1000ULL;
    CHECK( time_of(&clock, &exact) == SOON + 3600 - 36 );
    CHECK_FALSE( exact );

    // A stale time: counted less 1% already.
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    p.now_ms += 2ULL * 86400 * 1000;
    ebisu_clock_forget_time(&clock.base);
    CHECK( time_of(&clock, &exact) == LATER + 2 * 86400 - 1728 );
    CHECK_FALSE( exact );

    // After the monotonic clock went back, the time given.
    ebisu_server_clock_init(&clock, p.system.system());
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON) == true );
    p.now_ms -= 1000;
    ebisu_clock_forget_time(&clock.base);
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK_FALSE( exact );

    // A time given again is the time again.
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK( exact );
}

TEST_CASE( "A validity period is checked against the time, or against its floor" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    ebisu_server_clock_set_floor(&clock, SOON);

    // With the floor only: expired if it ended before it, valid otherwise,
    // even if it starts later, which could not be told from now.
    CHECK( ebisu_clock_check_validity(&clock.base, 0, SOON - 1) == EBISU_VALIDITY_EXPIRED );
    CHECK( ebisu_clock_check_validity(&clock.base, 0, SOON) == EBISU_VALIDITY_OK );
    CHECK( ebisu_clock_check_validity(&clock.base, LATER, LATER + 86400) == EBISU_VALIDITY_OK );

    // With the time: either end.
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    CHECK( ebisu_clock_check_validity(&clock.base, SOON, LATER - 1) == EBISU_VALIDITY_EXPIRED );
    CHECK( ebisu_clock_check_validity(&clock.base, LATER + 1, LATER + 86400) == EBISU_VALIDITY_NOT_YET_VALID );
    // Both ends inclusive.
    CHECK( ebisu_clock_check_validity(&clock.base, LATER, LATER) == EBISU_VALIDITY_OK );
    CHECK( ebisu_clock_check_validity(&clock.base, SOON, LATER + 86400) == EBISU_VALIDITY_OK );
}

TEST_CASE( "With a lock set, the clock is used and read only with it held" ) {
    Platform p;
    p.locked = true;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    ebisu_server_clock_set_lock(&clock, p.lock.get());

    int64_t now = 0;
    ebisu_server_clock_set_floor(&clock, SOON);
    REQUIRE( ebisu_server_clock_set_time(&clock, LATER) == true );
    ebisu_clock_get_time(&clock.base, &now);
    ebisu_clock_check_validity(&clock.base, 0, LATER);

    CHECK( p.locks == 4 );
    CHECK( p.depth == 0 );
    CHECK( p.max_depth == 1 );
    CHECK( p.clock_reads_unlocked == 0 );
}

TEST_CASE( "Without a monotonic clock, the clock has its floor alone, and takes no time" ) {
    ebisu_server_clock_t clock;
    CHECK_FALSE( ebisu_server_clock_init(&clock, NULL) );
    const int64_t floor = clock._floor;
    bool exact = true;
    CHECK_FALSE( ebisu_server_clock_set_time(&clock, SOON) );
    CHECK_FALSE( ebisu_server_clock_override_time(&clock, SOON) );
    clock.base.ops->stated(&clock.base, SOON);
    CHECK( time_of(&clock, &exact) == floor );
    CHECK_FALSE( exact );

    // A system without monotonic_ms is none either.
    khct::cb::SystemCtx system;
    system.ops.monotonic_ms = nullptr;
    CHECK_FALSE( ebisu_server_clock_init(&clock, system.system()) );
    CHECK_FALSE( ebisu_server_clock_override_time(&clock, floor - 1) );
    CHECK( time_of(&clock, &exact) == floor );
    CHECK_FALSE( exact );
}

TEST_CASE( "A time overriding the clock's is taken, one earlier than the floor too" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    bool exact = false;
    ebisu_server_clock_set_floor(&clock, LATER);

    // As a server's, earlier than the floor: not taken.
    CHECK_FALSE( ebisu_server_clock_set_time(&clock, SOON) );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK_FALSE( exact );

    // Overriding: the time, the floor lowered to it, and counted on.
    REQUIRE( ebisu_server_clock_override_time(&clock, SOON) );
    CHECK( clock._floor == SOON );
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK( exact );
    p.now_ms += 5000;
    CHECK( time_of(&clock, &exact) == SOON + 5 );
    CHECK( exact );

    // Over a time the clock has, later: taken, the floor left as it is.
    REQUIRE( ebisu_server_clock_override_time(&clock, LATER) );
    CHECK( clock._floor == SOON );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK( exact );

    // A time given like any other: the next a server states replaces it.
    REQUIRE( ebisu_server_clock_set_time(&clock, SOON + 100) );
    CHECK( time_of(&clock, &exact) == SOON + 100 );
    CHECK( exact );
}

TEST_CASE( "kii_set_clock() needs a clock that gives the time" ) {
    kii_t kii;
    kii_init(&kii);
    const ebisu_clock_ops_t no_now = { nullptr, nullptr, nullptr };
    ebisu_clock_t not_a_clock = { &no_now };
    CHECK( kii_set_clock(&kii, &not_a_clock) == KII_ERR_FAIL );
    CHECK( kii._clock == nullptr );
    ebisu_clock_t no_ops = { nullptr };
    CHECK( kii_set_clock(&kii, &no_ops) == KII_ERR_FAIL );
    CHECK( kii._clock == nullptr );

    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    CHECK( kii_set_clock(&kii, &clock.base) == KII_ERR_OK );
    CHECK( kii._clock == &clock.base );
    CHECK( kii_set_clock(&kii, NULL) == KII_ERR_OK );
    CHECK( kii._clock == nullptr );
}

namespace {

// A kii_t whose requests are each answered, on a connection of their own, by
// the next of responses. It notes the time its clock has as each connection
// opens, as a socket callback checking the server's certificate would.
struct Client {
    char buff[1024];
    kii_t kii;
    ebisu_clock_t* clock;
    std::vector<std::string> responses;
    size_t next;
    std::string resp;
    size_t resp_pos;
    std::vector<std::pair<int64_t, bool> > times_at_connect;
    khct::cb::SockCtx http_ctx;

    explicit Client(ebisu_clock_t* c) : clock(c), next(0), resp_pos(0) {
        kii_init(&kii);
        kii_set_site(&kii, "api.kii.com");
        kii_set_app_id(&kii, "dummyAppID");
        kii_set_buff(&kii, buff, sizeof(buff));
        http_ctx.on_connect = [this](void*, const char*, unsigned int) {
            int64_t now = 0;
            bool exact = ebisu_clock_get_time(clock, &now);
            times_at_connect.push_back(std::make_pair(now, exact));
            resp = (next < responses.size()) ? responses[next] : std::string();
            ++next;
            resp_pos = 0;
            return EBISU_SOCK_OK;
        };
        http_ctx.on_send = [](void*, const char*, size_t length, size_t* out_sent_length) {
            *out_sent_length = length;
            return EBISU_SOCK_OK;
        };
        http_ctx.on_recv = [this](void*, char* buffer, size_t length_to_read, size_t* out_actual_length) {
            size_t n = resp.size() - resp_pos;
            if (n > length_to_read) { n = length_to_read; }
            memcpy(buffer, resp.data() + resp_pos, n);
            resp_pos += n;
            *out_actual_length = n;
            return EBISU_SOCK_OK;
        };
        http_ctx.on_close = [](void*) { return EBISU_SOCK_OK; };
        kii_set_http_socket(&kii, http_ctx.socket());
    }

    kii_code_t get() {
        kii_code_t res = kii_api_call_start(&kii, "GET", "/api/apps/dummyAppID", NULL, false);
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
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    Client c(&clock.base);
    REQUIRE( kii_set_clock(&c.kii, &clock.base) == KII_ERR_OK );
    c.responses = { response(date_header(SOON)), response(""), response(date_header(LATER)) };

    REQUIRE( c.get() == KII_ERR_OK );
    p.now_ms += 5000;
    // No Date in this one: the clock counts on from the last.
    REQUIRE( c.get() == KII_ERR_OK );
    p.now_ms += 1000;
    REQUIRE( c.get() == KII_ERR_OK );

    REQUIRE( c.times_at_connect.size() == 3 );
    CHECK( c.times_at_connect[0].first == clock._floor );
    CHECK_FALSE( c.times_at_connect[0].second );
    CHECK( c.times_at_connect[1].first == SOON + 5 );
    CHECK( c.times_at_connect[1].second );
    CHECK( c.times_at_connect[2].first == SOON + 6 );
    CHECK( c.times_at_connect[2].second );

    bool exact = false;
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK( exact );
}

TEST_CASE( "Every kii_t sharing a clock has the time one of them got" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    Client first(&clock.base);
    Client second(&clock.base);
    REQUIRE( kii_set_clock(&first.kii, &clock.base) == KII_ERR_OK );
    REQUIRE( kii_set_clock(&second.kii, &clock.base) == KII_ERR_OK );
    first.responses = { response(date_header(SOON)) };
    second.responses = { response("") };

    REQUIRE( first.get() == KII_ERR_OK );
    REQUIRE( second.get() == KII_ERR_OK );
    REQUIRE( second.times_at_connect.size() == 1 );
    CHECK( second.times_at_connect[0].first == SOON );
    CHECK( second.times_at_connect[0].second );
}

TEST_CASE( "A date earlier than the clock's floor does not set it" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    // Saved by the device: later than the server says, which khc's own floor
    // cannot know.
    ebisu_server_clock_set_floor(&clock, LATER);
    Client c(&clock.base);
    REQUIRE( kii_set_clock(&c.kii, &clock.base) == KII_ERR_OK );
    c.responses = { response(date_header(SOON)) };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == SOON );

    bool exact = true;
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK_FALSE( exact );
}

TEST_CASE( "A response over plain HTTP gives the clock no time" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    Client c(&clock.base);
    REQUIRE( kii_set_clock(&c.kii, &clock.base) == KII_ERR_OK );
    kii_enable_insecure_http(&c.kii, true);
    c.responses = { response(date_header(SOON)), response(date_header(LATER)) };

    REQUIRE( c.get() == KII_ERR_OK );
    // The response still says when it was sent, for what it is worth.
    CHECK( kii_get_resp_date(&c.kii) == SOON );
    bool exact = true;
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );

    // Over TLS again, the server's time is taken.
    kii_enable_insecure_http(&c.kii, false);
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( time_of(&clock, &exact) == LATER );
    CHECK( exact );
}

namespace {

// A clock that keeps the time itself, as the system's: it has no stated.
struct KeptClock {
    ebisu_clock_t base;
    int reads;
};

bool kept_now(ebisu_clock_t* self, int64_t* out_now) {
    ++EBISU_CONTAINER_OF(self, KeptClock, base)->reads;
    *out_now = LATER;
    return true;
}

const ebisu_clock_ops_t kept_ops = { kept_now, nullptr, nullptr };

} // namespace

TEST_CASE( "A clock that keeps the time itself is given none, and is read" ) {
    KeptClock kept = { { &kept_ops }, 0 };
    Client c(&kept.base);
    REQUIRE( kii_set_clock(&c.kii, &kept.base) == KII_ERR_OK );
    c.responses = { response(date_header(SOON)) };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( kii_get_resp_date(&c.kii) == SOON );
    REQUIRE( c.times_at_connect.size() == 1 );
    CHECK( c.times_at_connect[0].first == LATER );
    CHECK( c.times_at_connect[0].second );
    CHECK( kept.reads == 1 );
    // And forgetting its time leaves it as it is.
    ebisu_clock_forget_time(&kept.base);
    int64_t now = 0;
    CHECK( ebisu_clock_get_time(&kept.base, &now) );
    CHECK( now == LATER );
}

namespace {

// A clock of the application's over a time source of its own, an NTP client,
// keeping the time in a server clock it holds, as the kii README shows.
struct NtpClock {
    ebisu_clock_t base;
    ebisu_server_clock_t kept;
    int syncs_asked;
};

bool ntp_now(ebisu_clock_t* self, int64_t* out_now) {
    NtpClock* clock = EBISU_CONTAINER_OF(self, NtpClock, base);
    return ebisu_clock_get_time(&clock->kept.base, out_now);
}

void ntp_doubt(ebisu_clock_t* self) {
    NtpClock* clock = EBISU_CONTAINER_OF(self, NtpClock, base);
    ebisu_clock_forget_time(&clock->kept.base);
    ++clock->syncs_asked;
}

const ebisu_clock_ops_t ntp_ops = { ntp_now, nullptr, ntp_doubt };

} // namespace

TEST_CASE( "A clock of the application's keeps the time its own source gives" ) {
    Platform p;
    NtpClock ntp;
    ntp.base.ops = &ntp_ops;
    ntp.syncs_asked = 0;
    REQUIRE( ebisu_server_clock_init(&ntp.kept, p.system.system()) );
    int64_t now = 0;

    // Until the source gives a time, the floor.
    CHECK_FALSE( ebisu_clock_get_time(&ntp.base, &now) );
    CHECK( now == ntp.kept._floor );

    // The source's time, counted on.
    REQUIRE( ebisu_server_clock_set_time(&ntp.kept, SOON) );
    p.now_ms += 5000;
    CHECK( ebisu_clock_get_time(&ntp.base, &now) );
    CHECK( now == SOON + 5 );

    // Set on a kii_t, it is given no server's Date: it has no stated.
    Client c(&ntp.base);
    REQUIRE( kii_set_clock(&c.kii, &ntp.base) == KII_ERR_OK );
    c.responses = { response(date_header(LATER)) };
    REQUIRE( c.get() == KII_ERR_OK );
    REQUIRE( c.times_at_connect.size() == 1 );
    CHECK( c.times_at_connect[0].first == SOON + 5 );
    CHECK( c.times_at_connect[0].second );
    CHECK( ebisu_clock_get_time(&ntp.base, &now) );
    CHECK( now == SOON + 5 );

    // A socket doubting it reaches the application, which can ask its source
    // for the time, and the time kept is a lower bound until then.
    ebisu_clock_forget_time(&ntp.base);
    CHECK( ntp.syncs_asked == 1 );
    CHECK_FALSE( ebisu_clock_get_time(&ntp.base, &now) );
    CHECK( now <= SOON + 5 );
    CHECK( now >= SOON );

    // The source's next time is the time again.
    REQUIRE( ebisu_server_clock_set_time(&ntp.kept, LATER) );
    CHECK( ebisu_clock_get_time(&ntp.base, &now) );
    CHECK( now == LATER );
}

TEST_CASE( "Without a clock set, responses set none" ) {
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    Client c(&clock.base);
    c.responses = { response(date_header(SOON)) };
    REQUIRE( c.get() == KII_ERR_OK );
    bool exact = true;
    CHECK( time_of(&clock, &exact) == clock._floor );
    CHECK_FALSE( exact );
}

TEST_CASE( "A certificate renewed while the clock fell behind is refused once, then taken" ) {
    // The device's monotonic clock stopped while it slept for 10 days, during
    // which the server renewed its certificate: valid from SOON + 10 days.
    // A socket callback refusing it as not valid yet forgets the time, so the
    // next connection is checked against a lower bound and goes through, and
    // its response gives the clock the time again.
    Platform p;
    ebisu_server_clock_t clock;
    ebisu_server_clock_init(&clock, p.system.system());
    Client c(&clock.base);
    REQUIRE( kii_set_clock(&c.kii, &clock.base) == KII_ERR_OK );
    int64_t not_before = 0;
    const int64_t not_after = LATER + 365 * 86400;
    int refused = 0;
    auto check_certificate = c.http_ctx.on_connect;
    c.http_ctx.on_connect = [&](void* ctx, const char* host, unsigned int port) {
        if (ebisu_clock_check_validity(&clock.base, not_before, not_after) != EBISU_VALIDITY_OK) {
            ebisu_clock_forget_time(&clock.base);
            ++refused;
            return EBISU_SOCK_FAIL;
        }
        return check_certificate(ctx, host, port);
    };
    c.responses = {
        response(date_header(SOON)),
        response(date_header(SOON + 10 * 86400)),
        response(""),
    };

    // Before the sleep: the old certificate, and the time from the response.
    not_before = SOON - 86400;
    REQUIRE( c.get() == KII_ERR_OK );
    bool exact = false;
    REQUIRE( time_of(&clock, &exact) == SOON );
    REQUIRE( exact );

    // After it: the clock says SOON still, the certificate is newer.
    not_before = SOON + 10 * 86400;
    CHECK( c.get() != KII_ERR_OK );
    CHECK( refused == 1 );
    CHECK( time_of(&clock, &exact) == SOON );
    CHECK_FALSE( exact );

    // The next connection goes through, and its response sets the time.
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( refused == 1 );
    CHECK( time_of(&clock, &exact) == SOON + 10 * 86400 );
    CHECK( exact );

    // From then on, the renewed certificate is valid at the time.
    REQUIRE( c.get() == KII_ERR_OK );
    CHECK( refused == 1 );
}
